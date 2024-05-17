#include "xdma_mod.h"
#include "xdma_cdev.h" 
#include "version.h"
#include "xdma_netdev.h" 
#include "alinx_ptp.h"

static u32 read32(void * addr) {
        return ioread32(addr);
}

static void write32(u32 val, void * addr) {
        iowrite32(val, addr);
}

static void set_pps_pulse_at(struct xdma_dev *xdev, sysclock_t time) {
        write32((u32)(time >> 32), xdev->bar[0] + NEXT_PULSE_AT_HI);
        write32((u32)time, xdev->bar[0] + NEXT_PULSE_AT_LO);
}

static sysclock_t get_sys_clock(struct xdma_dev *xdev)
{
        timestamp_t clock;
        clock = ((u64)read32(xdev->bar[0] + SYS_CLOCK_HI) << 32) |
                read32(xdev->bar[0] + SYS_CLOCK_LO);

        return clock;
}

static timestamp_t alinx_get_timestamp(u64 sys_count, double ticks_scale, u64 offset)
{
        timestamp_t timestamp = ticks_scale * sys_count;

        return timestamp + offset;
}

static void set_pulse_at(struct ptp_device_data *ptp_data, sysclock_t sys_count)
{
        timestamp_t current_ns, next_pulse_ns;
        sysclock_t next_pulse_sysclock;
        struct xdma_dev *xdev = ptp_data->xdev;

        current_ns = alinx_get_timestamp(sys_count, ptp_data->ticks_scale, ptp_data->offset);;
        next_pulse_ns = current_ns - (current_ns % 1000000000) + 1000000000;
        next_pulse_sysclock = ((double)(next_pulse_ns - ptp_data->offset) / ptp_data->ticks_scale);

        set_pps_pulse_at(xdev, next_pulse_sysclock);
}

static void set_pps_cycle_1s(struct xdma_dev *xdev, u32 cycle_1s) {
        write32(cycle_1s, xdev->bar[0] + CYCLE_1S);
}

static void set_cycle_1s(struct ptp_device_data *ptp_data, u32 cycle_1s) {
        set_pps_cycle_1s(ptp_data->xdev, cycle_1s);
}

static int alinx_ptp_gettimex(struct ptp_clock_info *ptp, struct timespec64 *ts,
			  struct ptp_system_timestamp *sts)
{
        u64 clock, timestamp;
        unsigned long flags;

        struct ptp_device_data *ptp_data = container_of(
                                ptp,
                                struct ptp_device_data,
                                ptp_info);

        spin_lock_irqsave(&ptp_data->lock, flags);

        clock = get_sys_clock(ptp_data->xdev);

        timestamp = alinx_get_timestamp(clock, ptp_data->ticks_scale, ptp_data->offset);
        
        ts->tv_sec = timestamp / 1000000000;
        ts->tv_nsec = timestamp % 1000000000;

        spin_unlock_irqrestore(&ptp_data->lock, flags);

        return 0;
}

static int alinx_ptp_settime(struct ptp_clock_info *ptp, const struct timespec64 *ts)
{
        u64 hw_timestamp, host_timestamp, sys_clock;
        unsigned long flags;

        struct ptp_device_data *ptp_data = container_of(
                                ptp,
                                struct ptp_device_data,
                                ptp_info);

        struct xdma_dev *xdev = ptp_data->xdev;

        /* Get host timestamp */
        host_timestamp = (u64)ts->tv_sec * 1000000000 + ts->tv_nsec;

        spin_lock_irqsave(&ptp_data->lock, flags);

        ptp_data->ticks_scale = TICKS_SCALE;

        sys_clock = get_sys_clock(xdev);
        hw_timestamp = alinx_get_timestamp(sys_clock, ptp_data->ticks_scale, ptp_data->offset);

        ptp_data->offset = host_timestamp - hw_timestamp;

        set_cycle_1s(ptp_data, RESERVED_CYCLE);
        set_pulse_at(ptp_data, sys_clock);

        spin_unlock_irqrestore(&ptp_data->lock, flags);

        return 0;
}

static int alinx_ptp_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
        u64 sys_clock;
        unsigned long flags;

        struct ptp_device_data *ptp_data = container_of(
                                ptp,
                                struct ptp_device_data,
                                ptp_info);

        spin_lock_irqsave(&ptp_data->lock, flags);

        /* Adjust offset */
        ptp_data->offset += delta;

        /* Set pulse_at */
        sys_clock = get_sys_clock(ptp_data->xdev);
        set_pulse_at(ptp_data, sys_clock);

        spin_unlock_irqrestore(&ptp_data->lock, flags);

        return 0;
}

static int alinx_ptp_adjfine(struct ptp_clock_info *ptp, long scaled_ppm)
{
        u64 cur_timestamp, new_timestamp;
        u64 sys_clock;
        u32 cycle_1s;
        double diff;
        unsigned long flags;
        int is_negative = 0;

        struct ptp_device_data *ptp_data = container_of(
                                ptp,
                                struct ptp_device_data,
                                ptp_info);
        struct xdma_dev *xdev = ptp_data->xdev;

        spin_lock_irqsave(&ptp_data->lock, flags);

        sys_clock = get_sys_clock(xdev);

        if (scaled_ppm == 0) {
                goto exit;
        }

        cur_timestamp = alinx_get_timestamp(sys_clock, ptp_data->ticks_scale, ptp_data->offset);

        if (scaled_ppm < 0) {
                is_negative = 1;
                scaled_ppm = -scaled_ppm;
        }

        /* Adjust ticks_scale */
        diff = TICKS_SCALE * (double)scaled_ppm / (double)(1000000ULL << 16);
        ptp_data->ticks_scale = TICKS_SCALE + (is_negative ? - diff : diff);

        /* Adjust offset */
        new_timestamp = alinx_get_timestamp(sys_clock, ptp_data->ticks_scale, ptp_data->offset);
        ptp_data->offset += (cur_timestamp - new_timestamp);

        /* Adjust cycle_1s */
        cycle_1s = (double)1000000000 / ptp_data->ticks_scale;
        set_cycle_1s(ptp_data, cycle_1s);

        /* Set pulse_at */
        sys_clock = get_sys_clock(xdev);
        set_pulse_at(ptp_data, sys_clock);

exit:
        spin_unlock_irqrestore(&ptp_data->lock, flags);

        return 0;
}

struct ptp_clock_info ptp_clock_info_init(void) {
        struct ptp_clock_info info = {
                .owner = THIS_MODULE,
                .name = "ptp",
                .max_adj = 12500000,
                .n_ext_ts = 0,
                .pps = 0,
                .adjfine = alinx_ptp_adjfine,
                .adjtime = alinx_ptp_adjtime,
                .gettimex64 = alinx_ptp_gettimex,
                .settime64 = alinx_ptp_settime,
        };

        return info;
}

struct ptp_device_data *ptp_device_init(struct device *dev, struct xdma_dev *xdev) {

        struct ptp_device_data *ptp;
        struct timespec64 ts;

        ptp = kzalloc(sizeof(struct ptp_device_data), GFP_KERNEL);
        if (!ptp) {
                pr_err("Failed to allocate memory for ptp device\n");
                return NULL;
        }
        memset(ptp, 0, sizeof(struct ptp_device_data));

        ptp->ptp_info = ptp_clock_info_init();
        ptp->ticks_scale = TICKS_SCALE;

        spin_lock_init(&ptp->lock);

        ptp->xdev = xdev;

        ptp->ptp_clock = ptp_clock_register(&ptp->ptp_info, dev);
        if (IS_ERR(ptp->ptp_clock)) {
                pr_err("Failed to register ptp clock\n");
                kfree(ptp);
                return NULL;
        }

        /* Set offset, cycle_1s */
        ts = ktime_to_timespec64(ktime_get_real());
        alinx_ptp_settime(&ptp->ptp_info, &ts);

        return ptp;
}

void ptp_device_destroy(struct ptp_device_data *ptp_data) {
        ptp_clock_unregister(ptp_data->ptp_clock);
        kfree(ptp_data);
}
