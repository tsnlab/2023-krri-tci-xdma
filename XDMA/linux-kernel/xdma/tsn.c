#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/string.h>

#include "xdma_netdev.h"
#include "tsn.h"

#define NS_IN_1S 1000000000

struct timestamps {
	timestamp_t from;
	timestamp_t to;
	timestamp_t delay_from;
	timestamp_t delay_to;
};

static struct tsn_config tsn_config;
static uint64_t bytes_to_ns(uint64_t bytes);
static void spend_qav_credit(struct tsn_config* tsn_config, timestamp_t at, uint8_t vlan_prio, uint64_t bytes);
static void get_timestamps(struct timestamps* timestamps, const struct tsn_config* tsn_config, timestamp_t from, uint8_t vlan_prio, uint64_t bytes, bool consider_delay);

uint8_t tsn_get_prio(const uint8_t *payload) {
	uint16_t eth_type = ntohs(*(uint16_t*)(payload + 12)); // TODO: Do better
	if (eth_type == ETH_P_8021Q) {
		struct vlan_hdr* vlan = (struct vlan_hdr*)(payload + ETH_HLEN);
		return vlan->pcp;
	}
	//XXX: Or you can use skb->priority;
	return 0;
}

void tsn_fill_metadata(struct tsn_config* tsn_config, timestamp_t from, struct tx_buffer* tx_buf) {
	struct tx_metadata* metadata = (struct tx_metadata*)&tx_buf->metadata;

	uint8_t vlan_prio = tsn_get_prio(tx_buf->data);
	bool is_gptp = false; // TODO
	memset(metadata, 0, sizeof(struct tx_metadata));

	enum tsn_prio queue_prio;
	if (is_gptp) {
		queue_prio = TSN_PRIO_GPTP;
	} else if (vlan_prio > 0) {
		queue_prio = TSN_PRIO_VLAN;
	} else {
		queue_prio = TSN_PRIO_BE;
	}

	struct timestamps timestamps;

	uint64_t duration_ns = bytes_to_ns(metadata->frame_length);

	if (tsn_config->qbv.enabled == false && tsn_config->qav[vlan_prio].enabled == false) {
		// Don't care. Just fill in the metadata
		timestamps.from = tsn_config->total_available_at;
		timestamps.to = from + _DEFAULT_TO_MARGIN_;
		metadata->fail_policy = TSN_FAIL_POLICY_DROP;
	} else {
		if (tsn_config->qav[vlan_prio].enabled == true && tsn_config->qav[vlan_prio].available_at_ns > from) {
			from = tsn_config->qav[vlan_prio].available_at_ns;
		}
		bool consider_delay = (vlan_prio > 0 || is_gptp);
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
	tsn_config->vlan_available_at += duration_ns;
	tsn_config->total_available_at += duration_ns;
}

void tsn_init_configs(struct tsn_config* config) {
	memset(config, 0, sizeof(struct tsn_config));

	// Calculate Qbv cycle
	// TODO: Extract this to a function and call it when Qbv config is updated by user
	int i;
	uint64_t cycle = 0;
	for (i = 0; i < config->qbv.slot_count; i += 1) {
		cycle += config->qbv.slots[i].duration_ns;
	}
}

static uint64_t bytes_to_ns(uint64_t bytes) {
	// TODO: Get link speed
	uint64_t link_speed = 1000000000; // Assume 1Gbps
	return bytes * 8 * NS_IN_1S / link_speed;
}

static void spend_qav_credit(struct tsn_config* tsn_config, timestamp_t at, uint8_t vlan_prio, uint64_t bytes) {
	struct qav_state* qav = &tsn_config->qav[vlan_prio];

	if (qav->enabled == false) {
		return;
	}

	if (at < qav->last_update || at < qav->available_at_ns) {
		// Invalid
		pr_err("Invalid timestamp Qav spending");
		return;
	}

	uint64_t elapsed_from_last = at - qav->last_update;
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
	qav->updated_at = send_end;
	if (qav->credit < 0) {
		qav->available_at = send_end + -(qav->credit / qav->idle_slope);
	} else {
		qav->available_at = send_end;
	}
}

/**
 * Get timestamps for a frame based on Qbv configuration
 */
static void get_timestamps(struct timestamps* timestamps, const struct tsn_config* tsn_config, timestamp_t from, uint8_t vlan_prio, uint64_t bytes, bool consider_delay) {
	memset(timestamps, 0, sizeof(struct timestamps));
	struct qbv_config* qbv = &tsn_config->qbv;

	if (qbv->enabled == false) {
		// No Qbv. Just return the current time
		timestamps->from = from;
		timestamps->to = from - 1;
		// delay_* is pointless. Just set it to be right next to the frame
		timestamps->delay_from = timestamps->from;
		timestamps->delay_to = timestamps->delay_from - 1;
		return;
	}

	uint64_t duration_ns = bytes_to_ns(bytes);
	// TODO: Get the current Qbv slot

	// TODO: Check if vlan_prio is always open or always closed
	// TODO: Need to check if the slot is big enough to fit the frame. But, That is a user fault. Don't mind.

	uint64_t remainder = (from + qbv->start) % qbv->_cycle_ns;
	int slot_id = 0;
	while (remainder > qbv->slots[slot_id].duration_ns) {
		remainder -= qbv->slots[slot_id].duration_ns;
		slot_id += 1;
	}

	// 1. Find from
	if (qbv->slots[slot_id].opened_prios[vlan_prio] == true) {
		// The slot is opened for this priority
		timestamps->from = from;
		timestamps->to = from - remainder + qbv->slots[slot_id].duration_ns;
	} else {
		// The slot is not opened for this priority. Find the next opened slot
		timestamps->from = from - remainder;
		while (qbv->slots[slot_id].opened_prios[vlan_prio] == false) {
			timestamps->from += qbv->slots[slot_id].duration_ns;
			timestamps->to = from + qbv->slots[slot_id].duration_ns;
			slot_id = (slot_id + 1) % qbv->slot_count;
		}
	}

	// 2. Find to
	while (qbv->slots[(slot_id + 1) % qbv->slot_count].opened_prios[vlan_prio] == true) {
		slot_id = (slot_id + 1) % qbv->slot_count;
		timestamps->to += qbv->slots[slot_id].duration_ns;
	}

	if (consider_delay) {
		timestamps->delay_from = timestamps->to;
		slot_id = (slot_id + 1) % qbv->slot_count;
		// 3. Find delay_from
		while (qbv->slots[slot_id].opened_prios[vlan_prio] == false) {
			timestamps->delay_from += qbv->slots[slot_id].duration_ns;
			timestamps->to = from + qbv->slots[slot_id].duration_ns;
			slot_id = (slot_id + 1) % qbv->slot_count;
		}

		// 3. Find delay_to
		while (qbv->slots[(slot_id + 1) % qbv->slot_count].opened_prios[vlan_prio] == true) {
			slot_id = (slot_id + 1) % qbv->slot_count;
			timestamps->delay_to += qbv->slots[slot_id].duration_ns;
		}
	} else {
		timestamps->delay_from = 0;
		timestamps->delay_to = 0;
	}

	// Adjust by frame size
	timestamps->to -= duration_ns;
	if (consider_delay) {
		timestamps->delay_to -= duration_ns;
	}
}
