#ifndef __IOCTL_XDMA_H__
#define __IOCTL_XDMA_H__

int xdma_api_ioctl_perf_start(char *devname, int size);
int xdma_api_ioctl_perf_get(char *devname, struct xdma_performance_ioctl *perf);
int xdma_api_ioctl_perf_stop(char *devname, struct xdma_performance_ioctl *perf);
int xdma_api_ioctl_get_max_buf_length(char *devname, int * max_buf_size);
int xdma_api_ioctl_print_engine(char *devname, int * version);
int xdma_api_ioctl_thread_start(char *devname, int * version);
int xdma_api_ioctl_thread_stop(char *devname, int * version);
int xdma_api_ioctl_thread_init(char *devname, int BufferCount);
int xdma_api_ioctl_thread_exit(char *devname, int * version);
int xdma_api_ioctl_bd_insert(char *devname, int * version);
int xdma_api_ioctl_bd_set_buffer_address(char *devname, int id, char *appBd);
int xdma_api_ioctl_bd_get_buffer_address(char *devname, int id, char *appBd);

#endif // __IOCTL_XDMA_H__
