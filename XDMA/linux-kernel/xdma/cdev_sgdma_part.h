#ifndef __CDEV_SGDMA_PART_H__
#define __CDEV_SGDMA_PART_H__

struct xdma_thread_init_ioctl {
    int buffer_count;
};

struct xdma_bd_set_bufAddr_ioctl {
    int id;
    char *buffer;
    unsigned long user_address;
};

struct xdma_bd_get_bufAddr_ioctl {
    int id;
    char *buffer;
    unsigned long user_address;
};

#define XAXIDMA_BD_NUM_WORDS        16U
struct struct_Bd{
    struct XDma_Bd *nextBd;
    char *buffer;
    unsigned long user_address;
    int  id; 
    int  status; /* 0: created, 1: ready, 2: using */
};

struct XDma_Bd {
    union {
        uint32_t arr_Bd[XAXIDMA_BD_NUM_WORDS];
        struct struct_Bd s_Bd;
    };
};


#if 1 // 20230830 POOKY TSNLAB
#define IOCTL_XDMA_MAX_BUF_LEN_GET _IOR('q', 9, int)
#define IOCTL_XDMA_PRN_ENGINE_INFO _IOR('q', 10, int)
#define IOCTL_XDMA_THREAD_START _IOR('q', 11, int)
#define IOCTL_XDMA_THREAD_STOP  _IOR('q', 12, int)
#define IOCTL_XDMA_THREAD_INIT  _IOW('q', 13, struct xdma_thread_init_ioctl *)
#define IOCTL_XDMA_THREAD_EXIT  _IOR('q', 14, int)
#define IOCTL_XDMA_BD_INSERT    _IOW('q', 15, struct XDma_Bd *)
#define IOCTL_XDMA_BD_SET_ADDR  _IOW('q', 16, struct xdma_bd_set_bufAddr_ioctl *)
#define IOCTL_XDMA_BD_GET_ADDR  _IOR('q', 17, struct xdma_bd_get_bufAddr_ioctl *)
#endif

#endif /* __CDEV_SGDMA_PART_H__ */
