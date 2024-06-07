#include "alinx_arch.h"
#include "libxdma.h"

#ifdef __linux__

#include <linux/io.h>

u32 read32(void * addr) {
        return ioread32(addr);
}

void write32(u32 val, void * addr) {
        iowrite32(val, addr);
}

#elif defined __ZEPHYR__

#include <zephyr/sys/sys_io.h>

u32 read32(void * addr) {
        return sys_read32((mem_addr_t)addr);
}

void write32(u32 val, void * addr) {
        sys_write32(val, (mem_addr_t)addr);
}

#else

#error unsupported os

#endif

void alinx_set_pulse_at(struct pci_dev *pdev, sysclock_t time) {
        struct xdma_dev* xdev = xdev_find_by_pdev(pdev);
        write32((u32)(time >> 32), xdev->bar[0] + REG_NEXT_PULSE_AT_HI);
        write32((u32)time, xdev->bar[0] + REG_NEXT_PULSE_AT_LO);
}

sysclock_t alinx_get_sys_clock(struct pci_dev *pdev) {
        struct xdma_dev* xdev = xdev_find_by_pdev(pdev);
        timestamp_t clock;
        clock = ((u64)read32(xdev->bar[0] + REG_SYS_CLOCK_HI) << 32) |
                read32(xdev->bar[0] + REG_SYS_CLOCK_LO);

        return clock;
}

void alinx_set_cycle_1s(struct pci_dev *pdev, u32 cycle_1s) {
        struct xdma_dev* xdev = xdev_find_by_pdev(pdev);
        write32(cycle_1s, xdev->bar[0] + REG_CYCLE_1S);
}

u32 alinx_get_cycle_1s(struct pci_dev *pdev) {
        struct xdma_dev* xdev = xdev_find_by_pdev(pdev);
        u32 ret = read32(xdev->bar[0] + REG_CYCLE_1S);
        return ret ? ret : RESERVED_CYCLE;
}
