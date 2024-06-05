#ifndef ALINX_PTP_H
#define ALINX_PTP_H

#include <linux/ptp_clock_kernel.h>
#include "libxdma.h"

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

struct ptp_device_data *ptp_device_init(struct device *dev, struct xdma_dev *xdev);
void ptp_device_destroy(struct ptp_device_data *ptp);

u32 ptp_get_cycle_1s(struct ptp_device_data *ptp);

#endif /* ALINX_PTP_H */
