#ifndef __DLA_INTF_H__
#define __DLA_INTF_H__
#include <ftd2xx.h>

struct dla_intf;

struct dla_cmd {
    int (*func)(struct dla_intf * intf, int argc, char ** argv);
    const char * name;
    const char * desc;   
};

struct dla_intf_support {
    const char * name;
    int supported;
};

struct dla_intf {     
    char name[16];
    char desc[128];
    char *devfile;
    int fd;
    int port;
    FT_HANDLE ftHandle;
    int opened;
    int abort;
    int noanswer;
    struct dla_cmd * cmdlist;
};

#endif // __DLA_INTF_H__
