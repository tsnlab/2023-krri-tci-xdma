#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/string.h>

#include "xdma_netdev.h"
#include "tsn.h"

#define NS_IN_1S 1000000000

static struct tsn_config tsn_config;
static uint64_t bytes_to_ns(uint64_t bytes);
static void spend_qav_credit(struct tsn_config* tsn_config, timestamp_t at, uint8_t vlan_prio, uint64_t bytes);

uint8_t tsn_get_prio(const uint8_t *payload) {
	uint16_t eth_type = ntohs(*(uint16_t*)(payload + 12)); // TODO: Do better
	if (eth_type == ETH_P_8021Q) {
		struct vlan_hdr* vlan = (struct vlan_hdr*)(skb->data + ETH_HLEN);
		return vlan->pcp;
	}
	//XXX: return skb->priority;
	return 0;
}

void tsn_fill_metadata(struct tsn_config* tsn_config, struct tx_buffer* tx_buf) {
	struct tx_metadata* metadata = tx_buf->metadata;

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

	// TODO: Calculate ticks
	timestamp_t from, to, d_from, d_to;

	uint64_t duration_ns = bytes_to_ns(metadata->frame_length);

	if (tsn_config->qbv.enabled == false && tsn_config->qav[vlan_prio].enabled == false) {
		// Don't care
		// TODO: Fill in the metadata
		from = tsn_config->total_available_at;
		to = from + BE_MARGIN_NS;
		metadata.fail_policy = TSN_FAIL_POLICY_DROP;
	} else {
		from = tsn_config->qav[vlan_prio].available_at_ns;
		from = ... // TODO: Closest qbv open
		to = // TODO: Closest qbv close
		if (vlan_prio > 0 || is_gptp) {
			d_from = ... // TODO: Next qbv open
			d_to = ... // TODO: Next qbv close
		}
		metadata.fail_policy = TSN_FAIL_POLICY_RETRY;
	}

	// TODO: Convert ns to sysclock
	metadata.from.tick = timestamp_to_sysclock(from);
	metadata.from.priority = queue_prio;
	metadata.to.tick = timestamp_to_sysclock(to);
	metadata.to.priority = queue_prio;
	metadata.delayed_from.tick = timestamp_to_sysclock(d_from);
	metadata.delayed_from.priority = queue_prio;
	metadata.delayed_to.tick = timestamp_to_sysclock(d_to);
	metadata.delayed_to.priority = queue_prio;

	// TODO: Update available_ats
	spend_qav_credit(tsn_config, from, vlan_prio, metadata->frame_length);
	tsn_config->vlan_available_at += duration_ns;
	tsn_config->total_available_at += duration_ns;
}

void tsn_init_configs(struct tsn_config* config) {
	memset(config, 0, sizeof(struct tsn_config));

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
