#ifndef EXAMPLE_PTP_H
#define EXAMPLE_PTP_H

#include <linux/ptp_clock_kernel.h>
#include "xdma_mod.h"
#include "libxdma.h"

#define NEXT_PULSE_AT_HI (0x002c)
#define NEXT_PULSE_AT_LO (0x0030)
#define CYCLE_1S (0x0034)
#define SYS_CLOCK_HI (0x0380)
#define SYS_CLOCK_LO (0x0384)

/* 125 MHz */
#define TICKS_SCALE ((double)8)
#define RESERVED_CYCLE (125000000)


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
};

struct ptp_device_data *ptp_device_init(struct device *dev, struct xdma_dev *xdev);
void ptp_device_destroy(struct ptp_device_data *ptp);

#endif /* EXAMPLE_PTP_H */
