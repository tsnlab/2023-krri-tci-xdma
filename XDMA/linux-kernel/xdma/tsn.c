#include <linux/if_ether.h>
#include <linux/string.h>

#include "alinx_ptp.h"
#include "xdma_netdev.h"
#include "tsn.h"

#define NS_IN_1S 1000000000
#define max(a, b) ((a) > (b) ? (a) : (b))

struct timestamps {
	timestamp_t from;
	timestamp_t to;
	timestamp_t delay_from;
	timestamp_t delay_to;
};

static bool is_gptp_packet(const uint8_t* payload);
static void bake_qbv_config(struct tsn_config* config);
static uint64_t bytes_to_ns(uint64_t bytes);
static void spend_qav_credit(struct tsn_config* tsn_config, timestamp_t at, uint8_t vlan_prio, uint64_t bytes);
static bool get_timestamps(struct timestamps* timestamps, const struct tsn_config* tsn_config, timestamp_t from, uint8_t vlan_prio, uint64_t bytes, bool consider_delay);

// HW Buffer tracker
static void append_buffer_track(struct buffer_tracker* tracker, sysclock_t free_at);
static void cleanup_buffer_track(struct buffer_tracker* tracker, sysclock_t now);

static sysclock_t timestamp_to_sysclock(timestamp_t timestamp) {
	// TODO: Read CLOCK_1S from register
	return timestamp / 8;
}

uint8_t tsn_get_vlan_prio(const uint8_t* payload) {
	struct ethhdr* eth = (struct ethhdr*)payload;
	uint16_t eth_type = eth->h_proto;
	if (eth_type == ETH_P_8021Q) {
		struct tsn_vlan_hdr* vlan = (struct tsn_vlan_hdr*)(payload + ETH_HLEN);
		return vlan->pcp;
	}
	//XXX: Or you can use skb->priority;
	return 0;
}

static bool is_gptp_packet(const uint8_t* payload) {
	struct ethhdr* eth = (struct ethhdr*)payload;
	uint16_t eth_type = eth->h_proto;
	if (eth_type == ETH_P_8021Q) {
		struct tsn_vlan_hdr* vlan = (struct tsn_vlan_hdr*)(eth + 1);
		eth_type = vlan->pid;
	}

	return eth_type == ETH_P_1588;
}

/**
 * Fill in the time related metadata of a frame
 * @param tsn_config: TSN configuration
 * @param now: Current time
 * @param tx_buf: The frame to be sent
 * @return: true if the frame reserves timestamps, false is for drop
 */
bool tsn_fill_metadata(struct tsn_config* tsn_config, timestamp_t now, struct sk_buff* skb) {
	struct tx_buffer* tx_buf = (struct tx_buffer*)skb->data;
	struct tx_metadata* metadata = (struct tx_metadata*)&tx_buf->metadata;
	struct buffer_tracker* buffer_tracker = &tsn_config->buffer_tracker;

	cleanup_buffer_track(buffer_tracker, timestamp_to_sysclock(now));

	uint8_t vlan_prio = tsn_get_vlan_prio(tx_buf->data);
	bool is_gptp = is_gptp_packet(tx_buf->data);

	enum tsn_prio queue_prio;
	if (is_gptp) {
		queue_prio = TSN_PRIO_GPTP;
	} else if (vlan_prio > 0) {
		queue_prio = TSN_PRIO_VLAN;
	} else {
		queue_prio = TSN_PRIO_BE;
	}
	bool consider_delay = (queue_prio != TSN_PRIO_BE);

	struct timestamps timestamps;
	timestamp_t from = now;

	uint64_t duration_ns = bytes_to_ns(metadata->frame_length);

	if (tsn_config->qbv.enabled == false && tsn_config->qav[vlan_prio].enabled == false) {
		// Don't care. Just fill in the metadata
		timestamps.from = tsn_config->total_available_at;
		timestamps.to = timestamps.from + _DEFAULT_TO_MARGIN_;
		metadata->fail_policy = TSN_FAIL_POLICY_DROP;
	} else {
		if (tsn_config->qav[vlan_prio].enabled == true && tsn_config->qav[vlan_prio].available_at > from) {
			from = tsn_config->qav[vlan_prio].available_at;
		}
		if (consider_delay) {
			// Check if queue is available
			if (buffer_tracker->count >= TSN_QUEUE_SIZE) {
				return false;
			}
		} else {
			// Best effort
			if (buffer_tracker->count >= BE_QUEUE_SIZE)
			from = max(from, tsn_config->total_available_at);
		}

		get_timestamps(&timestamps, tsn_config, from, vlan_prio, metadata->frame_length, consider_delay);
		metadata->fail_policy = consider_delay ? TSN_FAIL_POLICY_RETRY : TSN_FAIL_POLICY_DROP;
	}

	// TODO: Convert ns to sysclock
	metadata->from.tick = timestamp_to_sysclock(timestamps.from);
	metadata->from.priority = queue_prio;
	metadata->to.tick = timestamp_to_sysclock(timestamps.to);
	metadata->to.priority = queue_prio;
	metadata->delay_from.tick = timestamp_to_sysclock(timestamps.delay_from);
	metadata->delay_from.priority = queue_prio;
	metadata->delay_to.tick = timestamp_to_sysclock(timestamps.delay_to);
	metadata->delay_to.priority = queue_prio;

	// Update available_ats
	spend_qav_credit(tsn_config, from, vlan_prio, metadata->frame_length);
	tsn_config->queue_available_at[queue_prio] += duration_ns;
	tsn_config->total_available_at += duration_ns;

	timestamp_t free_at = max(timestamps.to + duration_ns, tsn_config->total_available_at);
	append_buffer_track(buffer_tracker, timestamp_to_sysclock(free_at));

	return true;
}

void tsn_init_configs(struct tsn_config* config) {
	memset(config, 0, sizeof(struct tsn_config));

	// Example Qbv configuration
	if (false) {
		config->qbv.enabled = true;
		config->qbv.start = 0;
		config->qbv.slot_count = 2;
		config->qbv.slots[0].duration_ns = 500000000; // 500ms
		config->qbv.slots[0].opened_prios[0] = true;
		config->qbv.slots[1].duration_ns = 500000000; // 500ms
		config->qbv.slots[1].opened_prios[0] = false;
	}

	// Example Qav configuration
	if (false) {
		// 100Mbps on 1Gbps link
		config->qav[0].enabled = true;
		config->qav[0].hi_credit = +1000000;
		config->qav[0].lo_credit = -1000000;
		config->qav[0].idle_slope = 10;
		config->qav[0].send_slope = -90;
	}

	bake_qbv_config(config);
}

static void bake_qbv_config(struct tsn_config* config) {
	if (config->qbv.enabled == false) {
		return;
	}

	int slot_id, vlan_prio; // Iterators
	struct qbv_baked_config* baked = &config->qbv_baked;

	baked->cycle_ns = 0;

	// First slot
	for (vlan_prio = 0; vlan_prio < VLAN_PRIO_COUNT; vlan_prio += 1) {
		baked->prios[vlan_prio].slot_count = 1;
		baked->prios[vlan_prio].slots[0].opened = config->qbv.slots[0].opened_prios[vlan_prio];
	}

	for (slot_id = 0; slot_id < config->qbv.slot_count; slot_id += 1) {
		uint64_t slot_duration = config->qbv.slots[slot_id].duration_ns;
		baked->cycle_ns += slot_duration;
		for (vlan_prio = 0; vlan_prio < VLAN_PRIO_COUNT; vlan_prio += 1) {
			struct qbv_baked_prio* prio = &baked->prios[vlan_prio];
			if (prio->slots[prio->slot_count - 1].opened == config->qbv.slots[slot_id].opened_prios[vlan_prio]) {
				// Same as the last slot. Just increase the duration
				prio->slots[prio->slot_count - 1].duration_ns += slot_duration;
			} else {
				// Different from the last slot. Add a new slot
				prio->slots[prio->slot_count].opened = config->qbv.slots[slot_id].opened_prios[vlan_prio];
				prio->slots[prio->slot_count].duration_ns = slot_duration;
				prio->slot_count += 1;
			}
		}
	}

	// Adjust slot counts to be even number. Because we need to have open-close pairs
	for (vlan_prio = 0; vlan_prio < VLAN_PRIO_COUNT; vlan_prio += 1) {
		struct qbv_baked_prio* prio = &baked->prios[vlan_prio];
		if (prio->slot_count % 2 == 1) {
			prio->slots[prio->slot_count].opened = !prio->slots[prio->slot_count - 1].opened;
			prio->slots[prio->slot_count].duration_ns = 0;
			prio->slot_count += 1;
		}
	}
}

static uint64_t bytes_to_ns(uint64_t bytes) {
	// TODO: Get link speed
	uint64_t link_speed = 1000000000; // Assume 1Gbps
	return max(bytes, MIN_FRAME_SIZE) * 8 * NS_IN_1S / link_speed;
}

static void spend_qav_credit(struct tsn_config* tsn_config, timestamp_t at, uint8_t vlan_prio, uint64_t bytes) {
	struct qav_state* qav = &tsn_config->qav[vlan_prio];

	if (qav->enabled == false) {
		return;
	}

	if (at < qav->last_update || at < qav->available_at) {
		// Invalid
		pr_err("Invalid timestamp Qav spending");
		return;
	}

	uint64_t elapsed_from_last_update = at - qav->last_update;
	double earned_credit = (double)elapsed_from_last_update * qav->idle_slope;
	qav->credit += earned_credit;
	if (qav->credit > qav->hi_credit) {
		qav->credit = qav->hi_credit;
	}

	uint64_t sending_duration = bytes_to_ns(bytes);
	double spending_credit = (double)sending_duration * qav->send_slope;
	qav->credit += spending_credit;
	if (qav->credit < qav->lo_credit) {
		qav->credit = qav->lo_credit;
	}

	// Calulate next available time
	timestamp_t send_end = at + sending_duration;
	qav->last_update = send_end;
	if (qav->credit < 0) {
		qav->available_at = send_end + -(qav->credit / qav->idle_slope);
	} else {
		qav->available_at = send_end;
	}
}

/**
 * Get timestamps for a frame based on Qbv configuration
 * @param timestamps: Output timestamps
 * @param tsn_config: TSN configuration
 * @param from: The time when the frame is ready to be sent
 * @param vlan_prio: VLAN priority of the frame
 * @param bytes: Size of the frame
 * @param consider_delay: If true, calculate delay_from and delay_to
 * @return: true if the frame reserves timestamps, false is for drop
 */
static bool get_timestamps(struct timestamps* timestamps, const struct tsn_config* tsn_config, timestamp_t from, uint8_t vlan_prio, uint64_t bytes, bool consider_delay) {
	memset(timestamps, 0, sizeof(struct timestamps));
	struct qbv_config* qbv = &tsn_config->qbv;

	if (qbv->enabled == false) {
		// No Qbv. Just return the current time
		timestamps->from = from;
		timestamps->to = from - 1;
		// delay_* is pointless. Just set it to be right next to the frame
		timestamps->delay_from = timestamps->from;
		timestamps->delay_to = timestamps->delay_from - 1;
		return true;
	}

	const struct qbv_baked_config* baked = &tsn_config->qbv_baked;
	const struct qbv_baked_prio* baked_prio = &baked->prios[vlan_prio];
	uint64_t sending_duration = bytes_to_ns(bytes);

	// TODO: Need to check if the slot is big enough to fit the frame. But, That is a user fault. Don't mind for now

	uint64_t remainder = (from - qbv->start) % baked->cycle_ns;
	int slot_id = 0;
	int slot_count = baked_prio->slot_count;

	// Check if vlan_prio is always open or always closed
	if (slot_count == 1) {
		if (baked_prio->slots[0].opened == false) {
			// The only slot is closed. Drop the frame
			return false;
		}
		timestamps->from = from;
		timestamps->to = from + baked_prio->slots[0].duration_ns - sending_duration;
		if (consider_delay) {
			timestamps->delay_from = timestamps->from + baked_prio->slots[0].duration_ns;
			timestamps->delay_to = timestamps->delay_from + baked_prio->slots[0].duration_ns - sending_duration;
		}
		return true;
	}

	while (remainder > baked_prio->slots[slot_id].duration_ns) {
		remainder -= baked_prio->slots[slot_id].duration_ns;
		slot_id += 1;
	}

	// 1. "from"
	if (baked_prio->slots[slot_id].opened) {
		timestamps->from = from - remainder;
	} else {
		// Select next slot
		timestamps->from = from - remainder + baked_prio->slots[slot_id].duration_ns;
		slot_id = (slot_id + 1) % baked_prio->slot_count; // Opened slot
	}

	// 2. "to"
	timestamps->to = timestamps->from + baked_prio->slots[slot_id].duration_ns;

	if (consider_delay) {
		// 3. "delay_from"
		timestamps->delay_from = timestamps->from + baked_prio->slots[slot_id].duration_ns;
		slot_id = (slot_id + 1) % baked_prio->slot_count; // Closed slot
		timestamps->delay_from += baked_prio->slots[slot_id].duration_ns;
		slot_id = (slot_id + 1) % baked_prio->slot_count; // Opened slot
		// 4. "delay_to"
		timestamps->delay_to = timestamps->delay_from + baked_prio->slots[slot_id].duration_ns;
	}

	// Adjust times
	timestamps->from = max(timestamps->from, from); // If already in the slot
	timestamps->to -= sending_duration;
	if (consider_delay) {
		timestamps->delay_to -= sending_duration;
	}

	return true;
}

static void append_buffer_track(struct buffer_tracker* tracker, sysclock_t free_at) {
	tracker->free_at[tracker->head] = free_at;
	tracker->head = (tracker->head + 1) % HW_QUEUE_SIZE;
	tracker->count += 1;
}

static void cleanup_buffer_track(struct buffer_tracker* tracker, sysclock_t now) {
	while (tracker->count > 0 && tracker->free_at[tracker->tail] < now) {
		tracker->tail = (tracker->tail + 1) % HW_QUEUE_SIZE;
		tracker->count -= 1;
	}
}

int tsn_set_qav(struct tsn_config* config, struct tc_cbs_qopt_offload* qopt) {
	if (qopt->queue != 0) {
		return -EINVAL;
	}

	config->qav[qopt->queue].enabled = qopt->enable;
	config->qav[qopt->queue].hi_credit = qopt->hicredit;
	config->qav[qopt->queue].lo_credit = qopt->locredit;
	config->qav[qopt->queue].idle_slope = qopt->idleslope;
	config->qav[qopt->queue].send_slope = qopt->sendslope;

	return 0;
}

int tsn_set_qbv(struct tsn_config* config, struct tc_taprio_qopt_offload* qopt) {
	u32 i, j;

	if (!qopt->enable) {
		return 0;
	}

	if (qopt->num_entries > MAX_QBV_SLOTS) {
		return -EINVAL;
	}

	config->qbv.enabled = qopt->enable;
	config->qbv.start = qopt->base_time;
	config->qbv.slot_count = qopt->num_entries;

	for (i = 0; i < config->qbv.slot_count; i++) {
		// TODO: handle qopt->entries[i].command
		config->qbv.slots[i].duration_ns = qopt->entries[i].interval;
		for (j = 0; j < VLAN_PRIO_COUNT; j++) {
			config->qbv.slots[i].opened_prios[j] = (qopt->entries[i].gate_mask & (1 << j));
		}
	}

	bake_qbv_config(config);

	return 0;
}
