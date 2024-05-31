#pragma once

#include "alinx_ptp.h"

#define HW_QUEUE_SIZE (128)
#define BE_QUEUE_SIZE (HW_QUEUE_SIZE - 20)
#define TSN_QUEUE_SIZE (HW_QUEUE_SIZE - 2)

#define VLAN_PRIO_COUNT 8
#define TSN_PRIO_COUNT 8
#define MAX_QBV_SLOTS 20

#define MIN_FRAME_SIZE (8 + ETH_ZLEN + 4 + 12) // 8 bytes preamble, 60 bytes payload, 4 bytes FCS, 12 bytes interpacket gap

enum tsn_prio {
	TSN_PRIO_GPTP = 3,
	TSN_PRIO_VLAN = 5,
	TSN_PRIO_BE = 7,
};

enum tsn_fail_policy {
	TSN_FAIL_POLICY_DROP = 0,
	TSN_FAIL_POLICY_RETRY = 1,
};

struct qbv_slot {
	uint32_t duration_ns; // We don't support cycle > 1s
	bool opened_prios[VLAN_PRIO_COUNT];
};

struct qbv_config {
	bool enabled;
	timestamp_t start;
	struct qbv_slot slots[MAX_QBV_SLOTS];

	uint32_t slot_count;
};

struct qbv_baked_prio_slot {
	uint64_t duration_ns;
	bool opened;
};

struct qbv_baked_prio {
	struct qbv_baked_prio_slot slots[MAX_QBV_SLOTS];
	size_t slot_count;
};

struct qbv_baked_config {
	uint64_t cycle_ns;
	struct qbv_baked_prio prios[VLAN_PRIO_COUNT];
};

struct qav_state {
	bool enabled;
	int32_t idle_slope; // credits/ns
	int32_t send_slope; // credits/ns
	int32_t hi_credit;
	int32_t lo_credit;

	int32_t credit;
	timestamp_t last_update;
	timestamp_t available_at;
};

struct buffer_tracker {
	sysclock_t free_at[HW_QUEUE_SIZE];
	int head; // Insert
	int tail; // Remove
	int count;
}

struct tsn_config {
	struct qbv_config qbv;
	struct qbv_baked_config qbv_baked;
	struct qav_state qav[VLAN_PRIO_COUNT];
	struct buffer_tracker buffer_tracker;
	timestamp_t queue_available_at[TSN_PRIO_COUNT];
	timestamp_t total_available_at;
};

struct vlan_hdr {
	uint16_t pid;
	uint8_t pcp:3;
	uint8_t dei:1;
	uint16_t vid:12;
} __attribute__((packed, scalar_storage_order("big-endian")));

uint8_t tsn_get_vlan_prio(const uint8_t* payload);
bool tsn_fill_metadata(struct tsn_config* tsn_config, timestamp_t now, struct tx_buffer* tx_buf);
void tsn_init_configs(struct tsn_config* config);
