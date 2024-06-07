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

u32 read32(void * addr);
void write32(u32 val, void * addr);

void alinx_set_pulse_at(struct pci_dev *pdev, sysclock_t time);
sysclock_t alinx_get_sys_clock(struct pci_dev *pdev);
void alinx_set_cycle_1s(struct pci_dev *pdev, u32 cycle_1s);
u32 alinx_get_cycle_1s(struct pci_dev *pdev);

#endif  /* ALINX_ARCH_H */
