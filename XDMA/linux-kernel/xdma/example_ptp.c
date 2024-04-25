#include "xdma_mod.h"
#include "xdma_cdev.h" 
#include "version.h"
#include "xdma_netdev.h" 
#include "example_ptp.h"

static void set_pps_pulse_at(struct xdma_dev *xdev, sysclock_t time) {
        iowrite32((u32)(time >> 32), xdev->bar[0] + NEXT_PULSE_AT_HI);
        iowrite32((u32)time, xdev->bar[0] + NEXT_PULSE_AT_LO);
}

static sysclock_t get_sys_clock(struct xdma_dev *xdev)
{
        timestamp_t clock;
        clock = ((u64)ioread32(xdev->bar[0] + SYS_CLOCK_HI) << 32) |
                ioread32(xdev->bar[0] + SYS_CLOCK_LO);
        /*
        clock = ((u64)example_read_register(xdev->bar[0] + SYS_CLOCK_HI) << 32) |
                example_read_register(xdev->bar[0] + SYS_CLOCK_LO);
                */
        return clock;
}

static timestamp_t example_get_timestamp(u64 sys_count, double ticks_scale, u64 offset)
{
        timestamp_t timestamp = ticks_scale * sys_count;
        return timestamp + offset;
}

static void set_pulse_at(struct ptp_device_data *ptp_data, sysclock_t sys_count)
{
        struct xdma_dev *xdev = ptp_data->xdev;

        timestamp_t current_ns = example_get_timestamp(sys_count, ptp_data->ticks_scale, ptp_data->offset);;
        timestamp_t next_pulse_ns = current_ns - (current_ns % 1000000000) + 1000000000;
        sysclock_t next_pulse_sysclock = ((double)(next_pulse_ns - ptp_data->offset) / ptp_data->ticks_scale);

        set_pps_pulse_at(xdev, next_pulse_sysclock);
}

static int example_ptp_gettimex(struct ptp_clock_info *ptp, struct timespec64 *ts,
			  struct ptp_system_timestamp *sts)
{
        unsigned long flags;
        u64 clock;
        u64 timestamp;
        struct timespec64 sys;
        struct ptp_device_data *ptp_data = container_of(
                                ptp,
                                struct ptp_device_data,
                                ptp_info);

        spin_lock_irqsave(&ptp_data->lock, flags);

        //ptp_read_system_prets(sts);
        clock = get_sys_clock(ptp_data->xdev);
        //ptp_read_system_postts(sts);

        timestamp = example_get_timestamp(clock, ptp_data->ticks_scale, ptp_data->offset);
        
        ts->tv_sec = timestamp / 1000000000;
        ts->tv_nsec = timestamp % 1000000000;

        spin_unlock_irqrestore(&ptp_data->lock, flags);
        return 0;
}

static int example_ptp_settime(struct ptp_clock_info *ptp, const struct timespec64 *ts)
{
        long flags;
        struct ptp_device_data *ptp_data = container_of(
                                ptp,
                                struct ptp_device_data,
                                ptp_info);

        struct xdma_dev *xdev = ptp_data->xdev;
        u64 hw_timestamp;
        u64 host_timestamp;
        u64 sys_clock;

        /* Get host timestamp */
        host_timestamp = (u64)ts->tv_sec * 1000000000 + ts->tv_nsec;

        spin_lock_irqsave(&ptp_data->lock, flags);

        ptp_data->ticks_scale = TICKS_SCALE;

        sys_clock = get_sys_clock(xdev);
        hw_timestamp = example_get_timestamp(sys_clock, ptp_data->ticks_scale, ptp_data->offset);
        ptp_data->offset = host_timestamp - hw_timestamp;
        iowrite32(RESERVED_CYCLE, xdev->bar[0] + CYCLE_1S);
        set_pulse_at(ptp_data, sys_clock);
        spin_unlock_irqrestore(&ptp_data->lock, flags);
        return 0;
}

static int example_ptp_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
        int flags;
        struct ptp_device_data *ptp_data = container_of(
                                ptp,
                                struct ptp_device_data,
                                ptp_info);
        u32 sec, nsec;
        u64 timestamp;
        u64 sys_clock;

        spin_lock_irqsave(&ptp_data->lock, flags);

        /* adj offset */
        ptp_data->offset += delta;

        /* adj pulse_at */
        sys_clock = get_sys_clock(ptp_data->xdev);
        set_pulse_at(ptp_data, sys_clock);

        spin_unlock_irqrestore(&ptp_data->lock, flags);
        return 0;
}

static int XXX_example_ptp_adjfine(struct ptp_clock_info *ptp, long scaled_ppm)
{
        long flags;
        struct ptp_device_data *ptp_data = container_of(
                                ptp,
                                struct ptp_device_data,
                                ptp_info);
        struct xdma_dev *xdev = ptp_data->xdev;
        int is_negative = 0;
        u64 cur_timestamp;
        u64 new_timestamp;
        u64 sys_clock;
        u32 cycle_1s;
        double diff;
        double ticks_scale;

        spin_lock_irqsave(&ptp_data->lock, flags);

        sys_clock = get_sys_clock(xdev);

        if (scaled_ppm == 0) {
                goto exit;
        }

        cur_timestamp = example_get_timestamp(sys_clock, ptp_data->ticks_scale, ptp_data->offset);

        if (scaled_ppm < 0) {
                is_negative = 1;
                scaled_ppm = -scaled_ppm;
        }

        /* Adjust ticks_scale */
        diff = TICKS_SCALE * (double)scaled_ppm / (double)(1000000ULL << 16);
        ptp_data->ticks_scale = TICKS_SCALE + (is_negative ? - diff : diff);

        /* Adjust offset */
        new_timestamp = example_get_timestamp(sys_clock, ptp_data->ticks_scale, ptp_data->offset);
        ptp_data->offset += (cur_timestamp - new_timestamp);

        /* Adjust cycle_1s */
        cycle_1s = (double)1000000000 / ptp_data->ticks_scale;
        iowrite32((u32)cycle_1s, xdev->bar[0] + CYCLE_1S);

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
                .max_adj = 12500000, // 12500000,
                .n_ext_ts = 0,
                .pps = 0,
                .adjfine = XXX_example_ptp_adjfine,
                .adjtime = example_ptp_adjtime,
                .gettimex64 = example_ptp_gettimex,
                .settime64 = example_ptp_settime,
        };
        return info;
}

struct ptp_device_data *ptp_device_init(struct device *dev, struct xdma_dev *xdev) {

        struct ptp_device_data *ptp;
        spinlock_t lock;
        struct timespec64 ts;
        long flags;

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
        example_ptp_settime(&ptp->ptp_info, &ts);
        return ptp;
}

void ptp_device_destroy(struct ptp_device_data *ptp_data) {
       
        long flags;

        ptp_clock_unregister(ptp_data->ptp_clock);
        kfree(ptp_data);
}
