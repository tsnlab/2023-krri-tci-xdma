#ifndef ALINX_ARCH_H
#define ALINX_ARCH_H

#ifdef __linux__
#include <linux/types.h>
#elif defined __ZEPHYR__
typedef uint32_t u32
#else
#error unsupported os
#endif

u32 read32(void * addr);
void write32(u32 val, void * addr);

#endif  /* ALINX_ARCH_H */
