#ifndef ALINX_ARCH_H
#define ALINX_ARCH_H

#ifdef __linux__
#include <linux/types.h>
#elif defined __ZEPHYR__
typedef uint32_t u32
#else
#error unsupported os
#endif

#include <linux/pci.h>
#include <linux/ptp_clock_kernel.h>

#define REG_NEXT_PULSE_AT_HI 0x002c
#define REG_NEXT_PULSE_AT_LO 0x0030
#define REG_CYCLE_1S 0x0034
#define REG_SYS_CLOCK_HI 0x0380
#define REG_SYS_CLOCK_LO 0x0384

/* 125 MHz */
#define TICKS_SCALE 8.0
#define RESERVED_CYCLE 125000000

#define HW_QUEUE_SIZE (128)
#define BE_QUEUE_SIZE (HW_QUEUE_SIZE - 20)
#define TSN_QUEUE_SIZE (HW_QUEUE_SIZE - 2)

#define VLAN_PRIO_COUNT 8
#define TSN_PRIO_COUNT 8
#define MAX_QBV_SLOTS 20

#define MIN_FRAME_SIZE (8 + ETH_ZLEN + 4 + 12) // 8 bytes preamble, 60 bytes payload, 4 bytes FCS, 12 bytes interpacket gap

typedef u64 sysclock_t;
typedef u64 timestamp_t;

struct ptp_device_data {
        struct device *dev;
        struct ptp_clock *ptp_clock;
        struct ptp_clock_info ptp_info;
        struct xdma_dev *xdev;
        double ticks_scale;
        u64 offset;
        spinlock_t lock;
#ifdef __LIBXDMA_DEBUG__
        u32 ptp_id;
#endif
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
};

struct tsn_config {
	struct qbv_config qbv;
	struct qbv_baked_config qbv_baked;
	struct qav_state qav[VLAN_PRIO_COUNT];
	struct buffer_tracker buffer_tracker;
	timestamp_t queue_available_at[TSN_PRIO_COUNT];
	timestamp_t total_available_at;
};

u32 read32(void * addr);
void write32(u32 val, void * addr);

void alinx_set_pulse_at(struct pci_dev *pdev, sysclock_t time);
sysclock_t alinx_get_sys_clock(struct pci_dev *pdev);
void alinx_set_cycle_1s(struct pci_dev *pdev, u32 cycle_1s);
u32 alinx_get_cycle_1s(struct pci_dev *pdev);

#endif  /* ALINX_ARCH_H */
