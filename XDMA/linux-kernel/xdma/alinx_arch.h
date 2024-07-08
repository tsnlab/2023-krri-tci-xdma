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
#include <net/pkt_sched.h>

#define REG_NEXT_PULSE_AT_HI 0x002c
#define REG_NEXT_PULSE_AT_LO 0x0030
#define REG_CYCLE_1S 0x0034
#define REG_SYS_CLOCK_HI 0x0380
#define REG_SYS_CLOCK_LO 0x0384

#define REG_TX_TIMESTAMP_COUNT 0x0300
#define REG_TX_TIMESTAMP1_HIGH 0x0310
#define REG_TX_TIMESTAMP1_LOW 0x0314
#define REG_TX_TIMESTAMP2_HIGH 0x0320
#define REG_TX_TIMESTAMP2_LOW 0x0324
#define REG_TX_TIMESTAMP3_HIGH 0x0330
#define REG_TX_TIMESTAMP3_LOW 0x0334
#define REG_TX_TIMESTAMP4_HIGH 0x0340
#define REG_TX_TIMESTAMP4_LOW 0x0344

#define REG_TX_PACKETS 0x0200
#define REG_TX_DROP_PACKETS 0x0220
#define REG_NORMAL_TIMEOUT_COUNT 0x041c
#define REG_TO_OVERFLOW_POPPED_COUNT 0x0420
#define REG_TO_OVERFLOW_TIMEOUT_COUNT 0x0424

#define TX_QUEUE_COUNT 3

/* 125 MHz */
#define TICKS_SCALE 8.0
#define RESERVED_CYCLE 125000000

#define HW_QUEUE_SIZE (128)
#define BE_QUEUE_SIZE (HW_QUEUE_SIZE - 20)
#define TSN_QUEUE_SIZE (HW_QUEUE_SIZE - 2)
#define HW_QUEUE_SIZE_PAD 20

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

struct mqprio_config {
	bool enabled;
	u8 num_tc;
	u8 prio_tc_map[TC_QOPT_BITMASK + 1];
	u16 count[TC_QOPT_MAX_QUEUE];
	// Ignore offsets
	// u16 offset[TC_QOPT_MAX_QUEUE];
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
	uint64_t pending_packets;
	uint64_t last_tx_count;
};

struct tsn_config {
	struct qbv_config qbv;
	struct qbv_baked_config qbv_baked;
	struct qav_state qav[VLAN_PRIO_COUNT];
	struct mqprio_config mqprio;
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
timestamp_t alinx_read_tx_timestamp(struct pci_dev *pdev, int tx_id);
u32 alinx_get_tx_packets(struct pci_dev *pdev);
u32 alinx_get_tx_drop_packets(struct pci_dev *pdev);
u32 alinx_get_normal_timeout_packets(struct pci_dev *pdev);
u32 alinx_get_to_overflow_popped_packets(struct pci_dev *pdev);
u32 alinx_get_to_overflow_timeout_packets(struct pci_dev *pdev);

void dump_buffer(unsigned char* buffer, int len);

#endif  /* ALINX_ARCH_H */
