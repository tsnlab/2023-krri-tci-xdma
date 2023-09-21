#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <byteswap.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <stdarg.h>

#include "cdev_sgdma.h"
#include "api_xdma.h"


struct xdma_performance_ioctl perf;
struct xdma_thread_init_ioctl th_init;

int xdma_api_ioctl_device_open(char *devname, int *fd)
{
    *fd = open(devname, O_RDWR);
    if (*fd < 0) {
        debug_printf("FAILURE: Could not open %s.\n", devname);
        debug_printf("         Make sure xdma device driver is loaded and ");
        debug_printf("and you have access rights (maybe use sudo?).\n");
        return -1;
    }
	return 0;
}
int xdma_api_ioctl_perf_start(char *devname, int size) {

    int fd;
    int rc = 0;
  
    if(xdma_api_ioctl_device_open(devname, &fd)) {
		return -1;
	}

    perf.version = IOCTL_XDMA_PERF_V1;
    perf.transfer_size = size;
    rc = ioctl(fd, IOCTL_XDMA_PERF_START, &perf);
    if (rc == 0) {
        debug_printf("IOCTL_XDMA_PERF_START succesful.\n");
    } else {
        debug_printf("ioctl(..., IOCTL_XDMA_PERF_START) = %d\n", rc);
    }

    close(fd);

    return rc;
}

int xdma_api_ioctl_perf_get(char *devname, 
                            struct xdma_performance_ioctl *perf) {
    int fd;
    int rc = 0;
  
    if(xdma_api_ioctl_device_open(devname, &fd)) {
		return -1;
	}

    rc = ioctl(fd, IOCTL_XDMA_PERF_GET, perf);
    if (rc == 0) {
      debug_printf("IOCTL_XDMA_PERF_GET succesful.\n");
    } else {
      debug_printf("ioctl(..., IOCTL_XDMA_PERF_GET) = %d\n", rc);
      return -1;
    }
    debug_printf("perf.transfer_size = %d\n", perf->transfer_size);
    debug_printf("perf.iterations = %d\n", perf->iterations);
    debug_printf("(data transferred = %lld bytes)\n", 
            (long long)perf->transfer_size * (long long)perf->iterations);
    debug_printf("perf.clock_cycle_count = %lld\n", (long long)perf->clock_cycle_count);
    debug_printf("perf.data_cycle_count = %lld\n", (long long)perf->data_cycle_count);
    if (perf->clock_cycle_count && perf->data_cycle_count) {
        debug_printf("(data duty cycle = %lld%%)\n", 
                (long long)perf->data_cycle_count * 100 / (long long)perf->clock_cycle_count);
    }

    close(fd);

    return 0;
}

int xdma_api_ioctl_perf_stop(char *devname, struct xdma_performance_ioctl *perf)
{
    int fd;
    int rc = 0;
  
    if(xdma_api_ioctl_device_open(devname, &fd)) {
		return -1;
	}

    rc = ioctl(fd, IOCTL_XDMA_PERF_STOP, perf);
    if (rc == 0) {
      debug_printf("IOCTL_XDMA_PERF_STOP succesful.\n");
    } else {
      debug_printf("ioctl(..., IOCTL_XDMA_PERF_STOP) = %d\n", rc);
      return -1;
    }
    debug_printf("perf.transfer_size = %d bytes\n", perf->transfer_size);
    debug_printf("perf.iterations = %d\n", perf->iterations);
    debug_printf("(data transferred = %lld bytes)\n", 
                 (long long)perf->transfer_size * (long long)perf->iterations);
    debug_printf("perf.clock_cycle_count = %lld\n", (long long)perf->clock_cycle_count);
    debug_printf("perf.data_cycle_count = %lld\n", (long long)perf->data_cycle_count);
    if (perf->clock_cycle_count && perf->data_cycle_count) {
        debug_printf("(data duty cycle = %lld%%)\n", 
                 (long long)perf->data_cycle_count * 100 / (long long)perf->clock_cycle_count);
        debug_printf(" data rate ***** bytes length = %d, rate = %f \n", perf->transfer_size, 
                 (double)(long long)perf->data_cycle_count/(long long)perf->clock_cycle_count);
    }
    debug_printf("perf.pending_count = %lld\n", (long long)perf->pending_count);

    close(fd);

    return 0;
}

/* dma from device */
int xdma_api_ioctl_aperture_r()
{
    return 0;
}

/* dma to device */
int xdma_api_ioctl_aperture_w()
{
    return 0;
}

int xdma_api_ioctl_engine_addrmode_get(char *devname, int *mode) {

    int fd;
    int rc = 0;

    if(xdma_api_ioctl_device_open(devname, &fd)) {
		return -1;
	}

    rc = ioctl(fd, IOCTL_XDMA_ADDRMODE_GET, mode);
    if (rc == 0) {
        printf("IOCTL_XDMA_ADDRMODE_GET succesful(max_buf_size: %d).\n", *mode);
    } else {
        printf("ioctl(..., IOCTL_XDMA_ADDRMODE_GET) = %d\n", rc);
    }

    close(fd);

    return rc;
}

int xdma_api_ioctl_engine_addrmode_set(char *devname, int *mode) {

    int fd;
    int rc = 0;

    if(xdma_api_ioctl_device_open(devname, &fd)) {
		return -1;
	}

    rc = ioctl(fd, IOCTL_XDMA_ADDRMODE_SET, mode);
    if (rc == 0) {
        printf("IOCTL_XDMA_ADDRMODE_SET succesful(max_buf_size: %d).\n", *mode);
    } else {
        printf("ioctl(..., IOCTL_XDMA_ADDRMODE_SET) = %d\n", rc);
    }

    close(fd);

    return rc;
}

int xdma_api_ioctl_get_max_buf_length(char *devname, int * max_buf_size) {

    int fd;
    int rc = 0;

	*max_buf_size = 0;
  
    if(xdma_api_ioctl_device_open(devname, &fd)) {
		return -1;
	}

    rc = ioctl(fd, IOCTL_XDMA_MAX_BUF_LEN_GET, max_buf_size);

    close(fd);

    return rc;
}

struct XDma_Bd g_bd;

int xdma_api_ioctl_print_engine(char *devname, int * version) {

    int fd;
    int rc = 0;

	*version = 0;
  
    if(xdma_api_ioctl_device_open(devname, &fd)) {
		return -1;
	}

  //  rc = ioctl(fd, IOCTL_XDMA_PRN_ENGINE_INFO, version);
    memset(&g_bd, 0x00, sizeof(struct XDma_Bd));
	g_bd.s_Bd.buffer = (char *)&g_bd;
    printf("address of g_bd: %p\n", (void *)&g_bd);
    rc = ioctl(fd, IOCTL_XDMA_BD_INSERT, &g_bd);

    close(fd);

    return rc;
}

//int xdma_api_ioctl_thread_init(char *devname, int BufferCount) {
int xdma_api_ioctl_thread_init(int fd, int BufferCount) {

//    int fd;
    int rc = 0;

//    if(xdma_api_ioctl_device_open(devname, &fd)) {
//		return -1;
//	}

    th_init.buffer_count = BufferCount;
    rc = ioctl(fd, IOCTL_XDMA_THREAD_INIT, &th_init);

//    close(fd);

    return rc;
}

int xdma_api_ioctl_thread_exit(int fd) {

    int rc = 0;
	int version = 0;

    rc = ioctl(fd, IOCTL_XDMA_THREAD_EXIT, &version);

    return rc;
}

int xdma_api_ioctl_thread_start(int fd) {

    int rc = 0;
	int version;

    rc = ioctl(fd, IOCTL_XDMA_THREAD_START, &version);

    return rc;
}

int xdma_api_ioctl_thread_stop(int fd) {

    int rc = 0;
	int version = 0;

    rc = ioctl(fd, IOCTL_XDMA_THREAD_STOP, &version);

    return rc;
}

int xdma_api_ioctl_bd_insert(char *devname, int * version) {

    int fd;
    int rc = 0;

	*version = 0;
  
    if(xdma_api_ioctl_device_open(devname, &fd)) {
		return -1;
	}

    printf("address of g_bd: %p\n", (void *)&g_bd);
    rc = ioctl(fd, IOCTL_XDMA_BD_INSERT, &g_bd);

    close(fd);

    return rc;
}

int xdma_api_ioctl_print_io_cb(int fd, int * version) {

    int rc = 0;

	*version = 0;
  
    rc = ioctl(fd, IOCTL_XDMA_PRN_IO_CB, version);

    return rc;
}

int xdma_api_ioctl_bd_set_buffer_address(int fd, int len, char *buffer) {

    int rc = 0;
	struct xdma_bd_set_bufAddr_ioctl bdSet;

    bdSet.len = len;
    bdSet.buffer = buffer;
    bdSet.user_address = (unsigned long)buffer;

#if 0
    printf("%s - len: %4d, buffer_address: %p, user_address: %16lx\n", 
	      __func__, bdSet.len, bdSet.buffer, bdSet.user_address);
#endif
    rc = ioctl(fd, IOCTL_XDMA_BD_SET_ADDR, &bdSet);

    return rc;
}

char * xdma_api_ioctl_bd_get_buffer_address(int fd, int *len) {

    int rc = 0;
	struct xdma_bd_get_bufAddr_ioctl bdGet;

    memset(&bdGet, 0 , sizeof(struct xdma_bd_get_bufAddr_ioctl));

    rc = ioctl(fd, IOCTL_XDMA_BD_GET_ADDR, &bdGet);

	if(rc == 0) {
#if 0
		printf("%s - len: %4d, buffer_address: %p, user_address: %16lx\n", 
			  __func__, bdGet.len, bdGet.buffer, bdGet.user_address);
#endif
		*len = bdGet.len;
		return bdGet.buffer;
	}
	else {
        return NULL;
	}
}

