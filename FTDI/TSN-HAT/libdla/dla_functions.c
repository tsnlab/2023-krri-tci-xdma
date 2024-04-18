#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>
#include <math.h>
#include <version.h>
#include <ftd2xx.h>
#include <lib_menu.h>
#include <log.h>
#include <helper.h>
#include <dla_intf.h>
#include <dla_functions.h>
#include <dla_board.h>

extern int verbose;

struct register_info CFRB[] = {
    { 0x000, 0x0000, "Control FPGA Version Number", RO},
    { 0x001, 0x0000, "Control FPGA Control1",       RW},
    { 0x002, 0x0000, "Control FPGA Control2",       RW},
    { 0x003, 0x0000, "Scratch",                     RW},
    { 0x004, 0x0000, "Input Read Packet Count",     RO},
    { 0x005, 0x0000, "Input Write Packet Count",    RO},
    { 0x006, 0x0000, "Output Packet Count",         RO},
    { 0x010, 0x0000, "LED Control",                 RW},
    { 0x011, 0x0000, "Header Status1",              RO},
    { 0x012, 0x0000, "Header Status2",              RO},
    { 0x013, 0x0000, "Switch Status",               RO},
    { 0x020, 0x0000, "CFR FIFO Status1",            RO},
    { 0x021, 0x0000, "CFR FIFO Status2",            RO},
    { 0x022, 0x0000, "CFT FIFO Status1",            RO},
    { 0x023, 0x0000, "CFT FIFO Status2",            RO},
    { 0x000, 0x0000,  NULL,                         RO}
};

struct register_info HUIB[] = {
    { 0x100, 0x0000, "HUIB Control",        RW},
    { 0x101, 0x0000, "HUIB Status",         RO},
    { 0x104, 0x0000, "Input Packet Count",  RO},
    { 0x105, 0x0000, "Output Packet Count", RO},
    { 0x110, 0x0000, "HUR FIFO Status1",    RO},
    { 0x111, 0x0000, "HUR FIFO Status2",    RO},
    { 0x112, 0x0000, "HUT FIFO Status1",    RO},
    { 0x113, 0x0000, "HUT FIFO Status2",    RO},
    { 0x000, 0x0000,  NULL,                 RO}
};

struct register_info DAB[] = {
    { 0x200, 0x0000, "DAB Control",         RW},
    { 0x201, 0x0000, "DAB Status",          RO},
    { 0x204, 0x0000, "Input Packet Count",  RO},
    { 0x205, 0x0000, "Output Packet Count", RO},
    { 0x000, 0x0000,  NULL,                 RO}
};

struct register_info MFDB[] = {
    { 0x300, 0x0000, "MFDB Control",        RW},
    { 0x301, 0x0000, "MFDB Status",         RO},
    { 0x304, 0x0000, "Input Packet Count",  RO},
    { 0x305, 0x0000, "Output Packet Count", RO},
    { 0x310, 0x0000, "MFR FIFO Status1",    RO},
    { 0x311, 0x0000, "MFR FIFO Status2",    RO},
    { 0x312, 0x0000, "MFT FIFO Status1",    RO},
    { 0x313, 0x0000, "MFT FIFO Status2",    RO},
    { 0x000, 0x0000,  NULL,                 RO}
};

struct register_info SMCB[] = {
    { 0x400, 0x0000, "SMCB Control",       RW},
    { 0x401, 0x0000, "SMCB Status",        RO},
    { 0x404, 0x0000, "Input Packet Count", RO},
    { 0x410, 0x0000, "SMR FIFO Status1",   RO},
    { 0x411, 0x0000, "SMR FIFO Status2",   RO},
    { 0x000, 0x0000,  NULL,                RO}
};

struct register_info MFRB[] = {
    { 0x000, 0x0000, "Main FPGA Version Number", RO},
    { 0x001, 0x0000, "Main FPGA Control1",       RW},
    { 0x002, 0x0000, "Main FPGA Control2",       RW},
    { 0x003, 0x0000, "Scratch",                  RW},
    { 0x004, 0x0000, "Input Read Packet Count",  RO},
    { 0x005, 0x0000, "Input Write Packet Count", RO},
    { 0x006, 0x0000, "Output Packet Count",      RO},
    { 0x010, 0x0000, "LED Control",              RW},
    { 0x011, 0x0000, "Header Status1",           RO},
    { 0x012, 0x0000, "Header Status2",           RO},
    { 0x013, 0x0000, "Switch Status",            RO},
    { 0x020, 0x0000, "MFR FIFO Status1",         RO},
    { 0x021, 0x0000, "MFR FIFO Status2",         RO},
    { 0x022, 0x0000, "MFT FIFO Status1",         RO},
    { 0x023, 0x0000, "MFT FIFO Status2",         RO},
    { 0x000, 0x0000,  NULL,                      RO}
};

struct register_info DPIB[] = {
    { 0x100, 0x0000, "DPIB Control",        RW},
    { 0x101, 0x0000, "DPIB Status",         RO},
    { 0x104, 0x0000, "Input Packet Count",  RO},
    { 0x105, 0x0000, "Output Packet Count", RO},
    { 0x110, 0x0000, "DPR FIFO Status1",    RO},
    { 0x111, 0x0000, "DPR FIFO Status2",    RO},
    { 0x112, 0x0000, "DPT FIFO Status1",    RO},
    { 0x113, 0x0000, "DPT FIFO Status2",    RO},
    { 0x000, 0x0000,  NULL,                 RO}
};

struct register_info MIB[] = {
    { 0x200, 0x0000, "MIB Control",              RW},
    { 0x201, 0x0000, "MIB Status",               RO},
    { 0x204, 0x0000, "MIR1 Input Packet Count",  RO},
    { 0x205, 0x0000, "MIR1 Output Packet Count", RO},
    { 0x210, 0x0000, "MIR1 FIFO Status1",        RO},
    { 0x211, 0x0000, "MIR1 FIFO Status2",        RO},
    { 0x212, 0x0000, "MIT1 FIFO Status1",        RO},
    { 0x213, 0x0000, "MIT1 FIFO Status2",        RO},
    { 0x220, 0x0000, "MIR2 FIFO Status1",        RO},
    { 0x221, 0x0000, "MIR2 FIFO Status2",        RO},
    { 0x222, 0x0000, "MIT2 FIFO Status1",        RO},
    { 0x223, 0x0000, "MIT2 FIFO Status2",        RO},
    { 0x000, 0x0000,  NULL,                      RO}
};

struct register_info DLAB[] = {
    { 0x300, 0x0000, "DLAB Control",       RW},
    { 0x301, 0x0000, "DLAB Status",        RO},
    { 0x304, 0x0000, "Input Packet Count", RO},
    { 0x305, 0x0000, "Output Packet Count",RO},
    { 0x000, 0x0000,  NULL,                RO}
};

void dumpBuffer(unsigned char *buffer, int elements) {
    int j;

    for (j = 0; j < elements; j++) {
        if (j % 8 == 0) {
            if (j % 16 == 0) {
                printf("\n%p: ", &buffer[j]);
            } else {
                printf("   "); // Separate two columns of eight bytes
            }
        }
        printf("%02X ", (unsigned int)buffer[j]);
    }
    printf("\n\n");
}

FT_HANDLE open_FTDI_Port(int portNum, int baudRate);

FT_HANDLE open_FTDI(int p) {
    FT_STATUS  ftStatus = FT_OK;
    FT_HANDLE  ftHandle = NULL;

    ftStatus = FT_Open(p, &ftHandle);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, 
                "FT_Open(%d) failed, with error %d.", 
                p, (int)ftStatus);
        lprintf(LOG_ERR, 
                "Use lsmod to check if ftdi_sio (and usbserial) are present.");
        lprintf(LOG_ERR, 
                "If so, unload them using rmmod, as they conflict with ftd2xx.");
        return NULL;
    }

    return ftHandle;
}

int set_baudRate_FTDI(FT_HANDLE ftHandle, int baudRate) {
    FT_STATUS  ftStatus = FT_OK;

    ftStatus = FT_SetBaudRate(ftHandle, (ULONG)baudRate);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, "Failure.  FT_SetBaudRate(%d) returned %d.", 
                baudRate, (int)ftStatus);
        return -1;
    }
    return 0;
}

int reset_device_FTDI(FT_HANDLE ftHandle) {
    FT_STATUS  ftStatus = FT_OK;

    ftStatus = FT_ResetDevice(ftHandle);
    if (ftStatus != FT_OK) {
        printf("Failure.  FT_ResetDevice returned %d.\n", (int)ftStatus);
        return -1;
    }
    return 0;
}

int config_device_FTDI(FT_HANDLE ftHandle) {
    FT_STATUS  ftStatus = FT_OK;

    ftStatus = FT_SetDataCharacteristics(ftHandle,
                             FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);
    if (ftStatus != FT_OK) {
        printf("Failure.  FT_SetDataCharacteristics returned %d.\n", 
                (int)ftStatus);
        return -1;
    }

    // Indicate our presence to remote computer
    ftStatus = FT_SetDtr(ftHandle);
    if (ftStatus != FT_OK) {
        printf("Failure.  FT_SetDtr returned %d.\n", (int)ftStatus);
        return -1;
    }


    // Flow control is needed for higher baud rates
    ftStatus = FT_SetFlowControl(ftHandle, FT_FLOW_RTS_CTS, 0, 0);
    if (ftStatus != FT_OK) {
        printf("Failure.  FT_SetFlowControl returned %d.\n", 
                (int)ftStatus);
        return -1;
    }

    // Assert Request-To-Send to prepare remote computer
    ftStatus = FT_SetRts(ftHandle);
    if (ftStatus != FT_OK) {
        printf("Failure.  FT_SetRts returned %d.\n", (int)ftStatus);
        return -1;
    }

    ftStatus = FT_SetTimeouts(ftHandle, 3000, 3000);  // 3 seconds
    if (ftStatus != FT_OK) {
        printf("Failure.  FT_SetTimeouts returned %d\n", (int)ftStatus);
        return -1;
    }

    return 0;
}

int write_buffer_FTDI(FT_HANDLE ftHandle, char *writeBuffer, 
                      DWORD bytesToWrite, DWORD *dwBytesWritten) {
    FT_STATUS  ftStatus = FT_OK;

    if (verbose >= 2) {
        dumpBuffer((unsigned char *)writeBuffer, bytesToWrite);
    }

    ftStatus = FT_Write(ftHandle, writeBuffer, bytesToWrite, dwBytesWritten);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, "Failure.  FT_Write returned %d", (int)ftStatus);
        return -1;
    }

    if (*dwBytesWritten != bytesToWrite) {
        lprintf(LOG_ERR, "Failure.  FT_Write wrote %d bytes instead of %d.",
                (int)(*dwBytesWritten), (int)bytesToWrite);
        return -1;
    }
    return 0;
}

int check_readBuffer_FTDI(FT_HANDLE ftHandle, DWORD dwBytesRead, 
                          DWORD *dwRxSize, long int timeout) {
    struct timeval  startTime;
    FT_STATUS ftStatus = FT_OK;
    char buff[256];
    int  dotCnt=0, queueChecks = 0;

    /* Read */
    gettimeofday(&startTime, NULL);
    for(queueChecks = 0; 
        (*dwRxSize < dwBytesRead) && (ftStatus == FT_OK); queueChecks++) {
        // Periodically check for time-out 
        if (queueChecks % 128 == 0) {
            struct timeval now, elapsed;

            gettimeofday(&now, NULL);
            timersub(&now, &startTime, &elapsed);

            if (elapsed.tv_sec > timeout) {
                // We've waited too long.  Give up.
                lprintf(LOG_DEBUG, "\nTimed out after %ld seconds", 
                        elapsed.tv_sec);
                return -1;
            }

            // Display number of bytes D2XX has received
            memset(buff, 0, 256);
            if((dotCnt++) % 2) {
                sprintf(buff, "Number of bytes in D2XX receive-queue: %d.", 
                        (int)(*dwRxSize));
            } else {
                sprintf(buff, "Number of bytes in D2XX receive-queue: %d ", 
                        (int)(*dwRxSize));
            }

            if (verbose >= 2) {
                printf("%s\r", buff);
            }
            fflush(stdout);
        }

        ftStatus = FT_GetQueueStatus(ftHandle, dwRxSize);
        if (ftStatus != FT_OK) {
            lprintf(LOG_ERR, "\nFT_GetQueueStatus failed (%d).", (int)ftStatus);
            return -1;
        }
    }
    if (verbose >= 2) {
        printf("Number of bytes in D2XX receive-queue: %d\n", (int)(*dwRxSize));
    }

    return 0;
}

int read_buffer_FTDI(FT_HANDLE ftHandle, unsigned char *readBuffer,
                     DWORD bytesReceived, DWORD *bytesRead) {
    FT_STATUS ftStatus = FT_OK;

    // Then copy D2XX's buffer to ours.
    ftStatus = FT_Read(ftHandle, readBuffer, bytesReceived, bytesRead);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, "Failure.  FT_Read returned %d.", (int)ftStatus);
        return -1;
    }

    if (*bytesRead != bytesReceived) {
        lprintf(LOG_ERR, "Failure.  FT_Read only read %d (of %d) bytes.",
                (int)(*bytesRead), (int)bytesReceived);
        return -1;
    }

    return 0;
}

int transceive_data_with_dla_board(int p, char *wb, int tL, 
                                   char *rb, int rL, int br) {
    FT_HANDLE  ftHandle = NULL;
    DWORD     dwBytesWritten, dwBytesRead;
    int err = -1;
    DWORD    dwRxSize = 0;
    long int timeout = 5; // seconds

    if((p<0) || (p>16)) {
        p = 0;
    }
    if (br < 0) {
        br = 9600;
    }

    lprintf(LOG_DEBUG, "Trying FTDI device %d at %d baud.", p, br);

    ftHandle = open_FTDI(p);
    if(ftHandle == NULL) {
        lprintf(LOG_ERR, "Error - ftHandle is NULL.");
        goto exit_transceive_data;
    }

    if(set_baudRate_FTDI(ftHandle, br)) {
        goto exit_transceive_data;
    }

    lprintf(LOG_DEBUG, "Calling FT_Write with this write-buffer:");

    /* Write */
    if(write_buffer_FTDI(ftHandle,wb, (DWORD)tL, &dwBytesWritten )) {
        goto exit_transceive_data;
    }

    /* Read */
    if(check_readBuffer_FTDI(ftHandle, (DWORD)rL, &dwRxSize, timeout)){
        goto exit_transceive_data;
    } else {
        lprintf(LOG_DEBUG, "\nGot %d (of %d) bytes.", 
                (int)dwRxSize, (int)dwBytesWritten);
        if(dwRxSize > rL) {
            dwRxSize = rL;
        }
        if(read_buffer_FTDI(ftHandle, (unsigned char *)rb, dwRxSize, 
            &dwBytesRead)) {
            goto exit_transceive_data;
        }
        lprintf(LOG_DEBUG, "FT_Read read %d bytes.  Read-buffer is now:", 
                (int)dwBytesRead);
        if (verbose >= 2) {
            dumpBuffer((unsigned char *)rb, (int)dwBytesRead);
        }
        lprintf(LOG_DEBUG, "\n%s passed.", __func__);
        err = 0;
    }

exit_transceive_data:
    if (ftHandle != NULL) {
        FT_Close(ftHandle);
    }

    return err;
}


int transfer_data_to_dla_board(int portNum, char *buf, int len, int baudRate) {
    FT_STATUS  ftStatus = FT_OK;
    FT_HANDLE  ftHandle = NULL;
    int err = -1;
    DWORD bytesToWrite = len, bytesWritten = 0;

    if((portNum<0) || (portNum>16)) {
        portNum = 0;
    }
    if (baudRate < 0) {
        baudRate = 9600;
    }

    lprintf(LOG_DEBUG, "Trying FTDI device %d at %d baud.", portNum, baudRate);

    ftHandle = open_FTDI(portNum);
    if(ftHandle == NULL) {
        lprintf(LOG_ERR, "Error - ftHandle is NULL.");
        goto exit_transfer_data;
    }

    ftStatus = FT_ResetDevice(ftHandle);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, "Failure.  FT_ResetDevice returned %d.", (int)ftStatus);
        goto exit_transfer_data;
    }

    ftStatus = FT_SetBaudRate(ftHandle, (ULONG)baudRate);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, "Failure.  FT_SetBaudRate(%d) returned %d.",
                baudRate, (int)ftStatus);
        goto exit_transfer_data;
    }

    ftStatus = FT_SetDataCharacteristics(ftHandle, FT_BITS_8, 
                                         FT_STOP_BITS_1, FT_PARITY_NONE);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, "Failure.  FT_SetDataCharacteristics returned %d.", 
                (int)ftStatus);
        goto exit_transfer_data;
    }

    // Indicate our presence to remote computer
    ftStatus = FT_SetDtr(ftHandle);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, "Failure.  FT_SetDtr returned %d.", (int)ftStatus);
        goto exit_transfer_data;
    }

    // Flow control is needed for higher baud rates
    ftStatus = FT_SetFlowControl(ftHandle, FT_FLOW_RTS_CTS, 0, 0);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, "Failure.  FT_SetFlowControl returned %d.", 
                (int)ftStatus);
        goto exit_transfer_data;
    }

    // Assert Request-To-Send to prepare remote computer
    ftStatus = FT_SetRts(ftHandle);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, "Failure.  FT_SetRts returned %d.", (int)ftStatus);
        goto exit_transfer_data;
    }

    ftStatus = FT_SetTimeouts(ftHandle, 3000, 3000);  // 3 seconds
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, "Failure.  FT_SetTimeouts returned %d", 
                (int)ftStatus);
        goto exit_transfer_data;
    }

    if (verbose >= 2) {
        dumpBuffer((unsigned char *)buf, len);
    }

    ftStatus = FT_Write(ftHandle,
                        buf,
                        bytesToWrite,
                        &bytesWritten);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, "Failure.  FT_Write returned %d", (int)ftStatus);
        goto exit_transfer_data;
    }

    if (bytesWritten != bytesToWrite) {
        lprintf(LOG_ERR, "Failure.  FT_Write wrote %d bytes instead of %d.",
                (int)bytesWritten, (int)bytesToWrite);
        goto exit_transfer_data;
    }

    // Success
    err = 0;
    lprintf(LOG_DEBUG, "Successfully wrote %d bytes", (int)bytesWritten);

exit_transfer_data:
    if (ftHandle != NULL) {
        FT_Close(ftHandle);
    }

    return err;
}

void fill_board_message_header(bdMsgHdr_t *h, enum data_dest d) {
    h->sodL = SOD_LOW_BYTE;
    h->sodH = SOD_HIGH_BYTE;
    h->dst  = d;
}

void fill_board_message_length(bdMsgHdr_t *h, unsigned short l) {
    h->d_lenL = l & 0xF;
    h->d_lenH = (l >> 4) & 0xFF;
}

void fill_memoryVariable(mibMsg_t *v, int o, int a, 
                         int len, unsigned char *buf) {
    fill_board_message_header(&v->hdr, DST_MIB);    
    if(o) {
        v->op = OPR_WR;
    } else {
        v->op = OPR_RD;
    }

    v->mAddrL = a & 0xFF;
    v->mAddrM = (a >>  8) & 0xFF;
    v->mAddrH = (a >> 16) & 0xFF;
    v->mAddrT = (a >> 24) & 0xFF;

    if(o) {
        memcpy(v->data, buf, len);
    }

    fill_board_message_length(&v->hdr, (int)(sizeof(bdMsgHdr_t) + 5 + len));
}

int do_memory(int portNum, int BaudRate, int rw, int addr, int len, 
              unsigned char *pBuff) {
    int err = 0, l = len, seg=0, s = addr;
    unsigned char *buf;
    mibMsg_t tMsg, rMsg;

    buf = pBuff;
    while(l > 0) {
        memset(&tMsg, 0, sizeof(mibMsg_t));
        if(l >= MAX_MIB_DATA_LEN) {
            seg = MAX_MIB_DATA_LEN;
        } else {
            seg = l;
        }

        fill_memoryVariable(&tMsg, rw, s, seg, buf);
        if(rw) {
            if(transfer_data_to_dla_board(portNum, (char *)&tMsg, 
                           (int)(sizeof(bdMsgHdr_t) + 5 + seg), BaudRate)) {
                lprintf(LOG_ERR, "Fail to transfer_data_to_dla_board.");
                return -1;
            }
            usleep(1000);
        } else {
            memset(&rMsg, 0, sizeof(mibMsg_t));
            if(transceive_data_with_dla_board(portNum, (char *)&tMsg, 
                             (int)(sizeof(bdMsgHdr_t) + 5), (char *)&rMsg, 
                             (int)(sizeof(bdMsgHdr_t) + 5 + seg), BaudRate)) {
                lprintf(LOG_ERR, "Fail to transceive_data_with_dla_board.");
                return -1;
            }
            memcpy(buf, rMsg.data, seg);
        }

        l -= seg, s += seg;
        if(l) {
            buf += seg;
        }
    }

    return err;
}

int dla_memory(int portNum, int BaudRate, int rw, int addr, 
               int DataSize, char *InputFileName) {
    unsigned char *pBuff = NULL;
    FILE *fp = NULL;
    int  len = DataSize, err = -1;

    lprintf(LOG_DEBUG, 
            "%s(portNum(%d), BaudRate(%d), %s, addr(0x%04x), DataSize(0x%03x), InputFileName(%s))", 
            __func__, portNum, BaudRate,
            (rw == 0 ) ? "read" : "write", 
            addr, DataSize, InputFileName);

    pBuff = calloc(DataSize, sizeof(unsigned char));
    if(pBuff == NULL) {
        lprintf(LOG_ERR, "Fail to allocates memory.");
        goto exit_memory;
    }
    if(rw == 1) {    // write operation
        fp = fopen(InputFileName, "rb");
        if(fp == NULL) {
            lprintf(LOG_ERR, "Cannot open %s file !", InputFileName);
            goto exit_memory;
        }
        len = (int)fread(pBuff, sizeof(unsigned char), DataSize, fp);
        fclose(fp);
        if(len != DataSize)  {
            lprintf(LOG_NOTICE, "Data size changed from %d to %d.", 
                    DataSize, len);
        }
    }

    if(do_memory(portNum, BaudRate, rw, addr, len, pBuff)) {
        lprintf(LOG_ERR, "Fail to do_memory !");
        goto exit_memory;
    }

    if(rw == 0) {
        dumpBuffer(pBuff, len);
    }
    err =0;

exit_memory:
    if(pBuff != NULL) {
        free(pBuff);
    }

    return err;
}

void print_register_group(regInfo_t regs[], char *grpTitle) {
    int id;

    printf("\n/*** %s ***/\n", grpTitle);
    for(id=0; regs[id].name != NULL; id++) {
        printf("0x%04x 0x%04x %s %s\n", regs[id].a, regs[id].d,
               regs[id].rw == RO ? "R " : "RW", regs[id].name);
    }
}

void fill_registerVariable(cfrbMsg_t *v, int o, int f, int a, int d) {
    if(f) {
        fill_board_message_header(&v->hdr, DST_MFRB);
    } else {
        fill_board_message_header(&v->hdr, DST_CFRB);
    }
    fill_board_message_length(&v->hdr, 8);

    if(o) {
        v->op = OPR_WR;
    } else {
        v->op = OPR_RD;
    }

    v->rAddrL = a & 0x3F;
    v->rAddrH = (a >> 6) & 0xFF;
    if(o) {
        v->rDataL = d & 0xFF;
        v->rDataH = (d >> 8) & 0xFF;
    }
}

int show_registerList(int fpga) {
    if(fpga) {
        print_register_group(MFRB, 
                    "MFRB(Main FPGA Reg Block) Register Group");
        print_register_group(DPIB, 
                    "DPIB(Data Path IF Block) Register Group");
        print_register_group(MIB,  
                    "MIB(Memory IF Block) Register Group");
        print_register_group(DLAB, 
                    "DLAB (Deep Learning Accelerator Block) Register Group");
    }
    else {
        print_register_group(CFRB, 
                    "CFRB(Control FPGA Reg Block) Register Group");
        print_register_group(HUIB, 
                    "HUIB(Host USB IF Block) Register Group");
        print_register_group(DAB,  
                    "DAB(Data Analysis Block) Register Group");
        print_register_group(MFDB, 
                    "MFDB (Main FPGA Data path Block) Register Group");
        print_register_group(SMCB, 
                    "SMCB(Select Map Control Block) Register Group");
    }
    printf("\n");

    return 0;
}

int read_register(int portNum, int BaudRate, int fpga, int addr, int *v) {
    cfrbMsg_t tMsg, rMsg;
    int rA, rD;

    lprintf(LOG_DEBUG, "%s(portNum(%d), BaudRate(%d), %s, addr(0x%04x)", 
                       __func__, portNum, BaudRate,
                       (fpga == 0) ? "control FPGA" : "main FPGA", addr);

    memset(&tMsg, 0, sizeof(cfrbMsg_t));
    memset(&rMsg, 0, sizeof(cfrbMsg_t));
    fill_registerVariable(&tMsg, 0, fpga, addr, 0x00);
    if (verbose >= 2) {
        dumpBuffer((unsigned char *)&tMsg, (int)sizeof(cfrbMsg_t));
    }

    if(transceive_data_with_dla_board(portNum, (char *)&tMsg, 
                                 (int)sizeof(cfrbMsg_t), (char *)&rMsg, 
                                 (int)sizeof(cfrbMsg_t), BaudRate)) {
        lprintf(LOG_ERR, "Fail to transceive_data_with_dla_board.");
        return -1;
    }

    rA = rMsg.rAddrH;
    rA = ((rA << 6) & 0x3FC0) + (rMsg.rAddrL & 0x3F);
    rD = rMsg.rDataH;
    rD = ((rD << 8) & 0xFF00) + (rMsg.rDataL & 0xFF);
    *v = rD;
    if (verbose >= 2) {
        dumpBuffer((unsigned char *)&rMsg, (int)sizeof(cfrbMsg_t));
    }
    if(addr != rA) {
        lprintf(LOG_ERR, 
                "Error : The sender address(%03x) and the receiver address(%03x) are different", 
                addr, rA);
        return -1;
    }
    lprintf(LOG_DEBUG, 
            "[R] %s FPGA Register - Address: 0x%03x  Value: 0x%04x", 
            (fpga == 0) ? "control" : "   main", rA, rD);

    return 0;
}

int read_register_with_ftHandle(FT_HANDLE  ftHandle, int fpga, int addr, int *v) {
    cfrbMsg_t tMsg, rMsg;
    int rA, rD;
    DWORD dwBytesWritten, dwBytesRead;
    DWORD dwRxSize = 0;
    long int timeout = 5; // seconds

    lprintf(LOG_DEBUG, 
            "%s(%s, addr(0x%04x)", 
            __func__, (fpga == 0) ? "control FPGA" : "main FPGA", addr);

    memset(&tMsg, 0, sizeof(cfrbMsg_t));
    memset(&rMsg, 0, sizeof(cfrbMsg_t));
    fill_registerVariable(&tMsg, 0, fpga, addr, 0x00);

    /* Write */
    if(write_buffer_FTDI(ftHandle, (char *)&tMsg, 
                (DWORD)(int)sizeof(cfrbMsg_t), &dwBytesWritten )) {
        return -1;
    }

    /* Read */
    if(check_readBuffer_FTDI(ftHandle, (DWORD)(int)sizeof(cfrbMsg_t), 
                             &dwRxSize, timeout)){
        return -1;
    } else {
        lprintf(LOG_DEBUG, "\nGot %d (of %d) bytes.", (int)dwRxSize, (int)dwBytesWritten);
        if(dwRxSize > (int)sizeof(cfrbMsg_t)) {
            dwRxSize = (int)sizeof(cfrbMsg_t);
        }
        if(read_buffer_FTDI(ftHandle, (unsigned char *)&rMsg, dwRxSize, 
            &dwBytesRead)) {
            return -1;
        }
        lprintf(LOG_DEBUG, 
                "FT_Read read %d bytes.  Read-buffer is now:", 
                (int)dwBytesRead);
        lprintf(LOG_DEBUG, "\n%s passed.", __func__);
    }

    rA = rMsg.rAddrH;
    rA = ((rA << 6) & 0x3FC0) + (rMsg.rAddrL & 0x3F);
    rD = rMsg.rDataH;
    rD = ((rD << 8) & 0xFF00) + (rMsg.rDataL & 0xFF);
    *v = rD;
    if(addr != rA) {
        lprintf(LOG_ERR, 
                "Error : The sender address(%03x) and the receiver address(%03x) are different", 
                addr, rA);
        return -1;
    }
    lprintf(LOG_DEBUG, 
            "[R] %s FPGA Register - Address: 0x%03x  Value: 0x%04x", 
            (fpga == 0) ? "control" : "   main", rA, rD);

    return 0;
}

int write_register(int portNum, int BaudRate, int fpga, int addr, int data) {
    cfrbMsg_t tMsg;

    lprintf(LOG_DEBUG, 
            "%s(portNum(%d), BaudRate(%d), %s, addr(0x%04x), data(0x%04x)", 
            __func__, portNum, BaudRate,
            (fpga == 0) ? "control FPGA" : "main FPGA", addr, data);

    memset(&tMsg, 0, sizeof(cfrbMsg_t));
    fill_registerVariable(&tMsg, 1, fpga, addr, data);
    if(transfer_data_to_dla_board(portNum, (char *)&tMsg, 
                                  (int)sizeof(cfrbMsg_t), BaudRate)) {
        lprintf(LOG_ERR, "Fail to transfer_data_to_dla_board.");
        return -1;
    }

    lprintf(LOG_DEBUG, 
            "[W] %s FPGA Register - Address: 0x%03x  Value: 0x%04x", 
            (fpga == 0) ? "control" : "   main", addr, data);

    return 0;
}

int write_register_with_ftHandle(FT_HANDLE  ftHandle, int fpga, 
                                 int addr, int data) { 
    cfrbMsg_t tMsg;
    FT_STATUS  ftStatus = FT_OK;
    DWORD bytesToWrite, bytesWritten;

    lprintf(LOG_DEBUG, 
            "%s(%s, addr(0x%04x), data(0x%04x)", 
            __func__, (fpga == 0) ? "control FPGA" : "main FPGA", addr, data);

    memset(&tMsg, 0, sizeof(cfrbMsg_t));
    fill_registerVariable(&tMsg, 1, fpga, addr, data);

    bytesToWrite = (int)sizeof(cfrbMsg_t);
    bytesWritten = 0;
    ftStatus = FT_Write(ftHandle,
                        (char *)&tMsg,
                        bytesToWrite,
                        &bytesWritten);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, "Failure.  FT_Write returned %d", (int)ftStatus);
        return -1;
    }

    if (bytesWritten != bytesToWrite) {
        lprintf(LOG_ERR, "Failure.  FT_Write wrote %d bytes instead of %d.",
                         (int)bytesWritten, (int)bytesToWrite);
        return -1;
    } 

    lprintf(LOG_DEBUG, "[W] %s FPGA Register - Address: 0x%03x  Value: 0x%04x",
                       (fpga == 0) ? "control" : "   main", addr, data);

    return 0;
}

int read_group_register(int portNum, int BaudRate, int fpga, 
                        struct register_info *regGrp) {
    int id, v;

    for(id=0; regGrp[id].name != NULL; id++) {
        if(read_register(portNum, BaudRate, fpga, regGrp[id].a, &v)) {
            continue;
        }
        regGrp[id].d = (unsigned short)(v & 0xFFFF);
        lprintf(LOG_NOTICE, 
                "[R] %s FPGA Register - Address: 0x%03x  Value: 0x%04x", 
                (fpga == 0) ? "control" : "   main", regGrp[id].a, v);
    }
    return 0;
}

int write_group_register(int portNum, int BaudRate, int fpga, int data, 
                         struct register_info *regGrp) {
    int id, v;

    for(id=0; regGrp[id].name != NULL; id++) {
        if(regGrp[id].rw != RW) {
            continue;
        }
        if(write_register(portNum, BaudRate, fpga, regGrp[id].a, data)) {
            continue;
        }
        usleep(1000);
        if(read_register(portNum, BaudRate, fpga, regGrp[id].a, &v)) {
            continue;
        }
        regGrp[id].d = (unsigned short)(v & 0xFFFF);
        if(data != v) {
            lprintf(LOG_ERR, 
                    "Error %s FPGA Register - Address: 0x%03x  W-Value: 0x%04x R-Value: 0x%04x", 
                    (fpga == 0) ? "control" : "   main", regGrp[id].a, data, v);
            continue;
        }
        lprintf(LOG_NOTICE, 
                "[W] %s FPGA Register - Address: 0x%03x  Value: 0x%04x", 
                (fpga == 0) ? "control" : "   main", regGrp[id].a, data);
    }
    return 0;
}

int dla_register(int portNum, int BaudRate, int rw, int fpga, int addr, 
                 int data) {
    int rD, r;

    lprintf(LOG_DEBUG, 
            "%s(portNum(%d), BaudRate(%d), %s, %s, addr(0x%04x), data(0x%04x))", 
            __func__, portNum, BaudRate, 
            (rw == 0 ) ? "read" : ((rw == 1) ? "write" : "list"), 
            (fpga == 0) ? "control FPGA" : "main FPGA", addr, data);

    if(rw == 2) {
        return show_registerList(fpga);
    }

    if(rw) {
        if(addr == 0xFFFF) {
            if(fpga) {
                write_group_register(portNum, BaudRate, fpga, data, MFRB);
                write_group_register(portNum, BaudRate, fpga, data, DPIB);
                write_group_register(portNum, BaudRate, fpga, data, MIB);
                write_group_register(portNum, BaudRate, fpga, data, DLAB);
            } else {
                write_group_register(portNum, BaudRate, fpga, data, CFRB);
                write_group_register(portNum, BaudRate, fpga, data, HUIB);
                write_group_register(portNum, BaudRate, fpga, data, DAB);
                write_group_register(portNum, BaudRate, fpga, data, MFDB);
                write_group_register(portNum, BaudRate, fpga, data, SMCB);
            }
        } else {
            r = write_register(portNum, BaudRate, fpga, addr, data);
            if(r == 0) {
                lprintf(LOG_NOTICE, 
                        "[W] %s FPGA Register - Address: 0x%03x  Value: 0x%04x", 
                        (fpga == 0) ? "control" : "   main", addr, data);
            }

            return r;
        }
    } else {
        if(addr == 0xFFFF) {
            if(fpga) {
                read_group_register(portNum, BaudRate, fpga, MFRB);
                read_group_register(portNum, BaudRate, fpga, DPIB);
                read_group_register(portNum, BaudRate, fpga, MIB);
                read_group_register(portNum, BaudRate, fpga, DLAB);
            } else {
                read_group_register(portNum, BaudRate, fpga, CFRB);
                read_group_register(portNum, BaudRate, fpga, HUIB);
                read_group_register(portNum, BaudRate, fpga, DAB);
                read_group_register(portNum, BaudRate, fpga, MFDB);
                read_group_register(portNum, BaudRate, fpga, SMCB);
            }
        } else {
            r = read_register(portNum, BaudRate, fpga, addr, &rD);
            if(r == 0)
                lprintf(LOG_NOTICE, 
                        "[R] %s FPGA Register - Address: 0x%03x  Value: 0x%04x", 
                        (fpga == 0) ? "control" : "   main", addr, rD);
            return r;
        }
    }

    return 0;
}

int dla_loopback(int portNum, int DataSize, int BaudRate, char *InputFileName) {
    int       retCode = -1; // Assume failure
    DWORD     driverVersion = 0;
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle = NULL;
    size_t    bufferSize = DataSize;
    DWORD     bytesToWrite;
    DWORD     bytesWritten = 0;
    DWORD     bytesReceived = 0;
    DWORD     bytesRead = 0;
    struct timeval  startTime;
    int             journeyDuration;
    unsigned char  *writeBuffer = NULL;
    unsigned char  *readBuffer = NULL;
    size_t len;
    FILE   *rfp;

    lprintf(LOG_DEBUG, 
            "%s(portNum(%d), DataSize(%d), BaudRate(%d), InputFileName:%s))", 
            __func__, portNum, DataSize, BaudRate, InputFileName);
    writeBuffer = (unsigned char *)malloc((size_t)bufferSize);
    if (writeBuffer == NULL) {
        goto exit_loopback;
    }

    rfp = fopen(InputFileName, "rb");
    if(rfp == NULL) {
        lprintf(LOG_ERR, "Cannot open %s file !", InputFileName);
        goto exit_loopback;
    }

    len = fread(writeBuffer, sizeof(unsigned char), bufferSize, rfp);
    fclose(rfp);
    if (verbose >= 2) {
        dumpBuffer((unsigned char *)writeBuffer, bufferSize);
    }

    if((portNum<0) || (portNum>16)) {
        portNum = 0;
    }
    if (BaudRate < 0) {
        BaudRate = 9600;
    }

    lprintf(LOG_DEBUG, 
            "Trying FTDI device %d at %d baud.", portNum, BaudRate);

    ftHandle = open_FTDI(portNum);
    if(ftHandle == NULL) {
        lprintf(LOG_ERR, "Error - ftHandle is NULL.");
        goto exit_loopback;
    }

    ftStatus = FT_GetDriverVersion(ftHandle, &driverVersion);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, 
                "Failure.  FT_GetDriverVersion returned %d.", 
                (int)ftStatus);
        goto exit_loopback;
    }

    lprintf(LOG_NOTICE, "Using D2XX version %08x\n", driverVersion);

    if(reset_device_FTDI(ftHandle)) {
        goto exit_loopback;
    }
    if(config_device_FTDI(ftHandle)) {
        goto exit_loopback;
    }

    if(set_baudRate_FTDI(ftHandle, BaudRate)) {
        goto exit_loopback;
    }

    bytesToWrite = len;

    lprintf(LOG_NOTICE, 
            "\nBaud rate %d.  Writing %d bytes to loopback device...", 
            BaudRate, (int)bytesToWrite);

    if(write_buffer_FTDI(ftHandle, (char *)writeBuffer, (DWORD)bytesToWrite, 
        &bytesWritten )) {
        goto exit_loopback;
    }

    lprintf(LOG_DEBUG, "%d bytes written.", (int)bytesWritten);

    // Keep checking queue until D2XX has received all the bytes we wrote.
    // Estimate total time to write and read, so we can time-out.
    // Each byte has 8 data bits plus a stop bit and perhaps a 1-bit gap.
    journeyDuration = bytesWritten * (8 + 1 + 1) / (int)BaudRate;
    journeyDuration += 1;  // Round up
    journeyDuration *= 6;  // It's a return journey
    lprintf(LOG_DEBUG, "Estimate %d seconds remain.", journeyDuration);

    gettimeofday(&startTime, NULL);

    if(check_readBuffer_FTDI(ftHandle, (DWORD)bytesWritten, &bytesReceived, 
                             journeyDuration)){
        goto exit_loopback;
    } else {
        lprintf(LOG_DEBUG, 
                "\nGot %d (of %d) bytes.", 
                (int)bytesReceived, (int)bytesWritten);
        if(bytesReceived > bytesWritten) {
            bytesReceived = bytesWritten;
        }
        readBuffer = (unsigned char *)calloc(bytesReceived, sizeof(unsigned char));
        if (readBuffer == NULL) {
            lprintf(LOG_ERR, "Failed to allocate %d bytes.", bytesReceived);
            goto exit_loopback;
        }
        lprintf(LOG_DEBUG, "Calling FT_Read with this read-buffer:");

        if(read_buffer_FTDI(ftHandle, (unsigned char *)readBuffer, 
                            bytesReceived, &bytesRead)) {
            goto exit_loopback;
        }
    }

    if (0 != memcmp(writeBuffer, readBuffer, bytesRead)) {
        lprintf(LOG_ERR, 
                "Failure.  Read-buffer does not match write-buffer.");
        lprintf(LOG_ERR, 
                "Write buffer:");
        dumpBuffer(writeBuffer, bytesReceived);
        lprintf(LOG_ERR, "Read buffer:");
        dumpBuffer(readBuffer, bytesReceived);
        goto exit_loopback;
    }

    // Fail if D2XX's queue lacked (or had surplus) bytes.
    if (bytesReceived != bytesWritten) {
        lprintf(LOG_ERR, 
                "Failure.  D2XX received %d bytes but we expected %d.",
                (int)bytesReceived, (int)bytesWritten);
        dumpBuffer(readBuffer, bytesReceived);
        goto exit_loopback;
    }

    // Check that queue hasn't gathered any additional unexpected bytes
    bytesReceived = 4242; // deliberately junk
    ftStatus = FT_GetQueueStatus(ftHandle, &bytesReceived);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, 
                "Failure.  FT_GetQueueStatus returned %d.\n", (int)ftStatus);
        goto exit_loopback;
    }

    if (bytesReceived != 0) {
        lprintf(LOG_ERR, 
                "Failure.  %d bytes in input queue -- expected none.\n",
                (int)bytesReceived);
        goto exit_loopback;
    }

    // Success
    lprintf(LOG_NOTICE, "\nTest PASSED.\n");
    dumpBuffer((unsigned char *)readBuffer, bytesReceived);
    retCode = 0;

exit_loopback:
    free(readBuffer);
    free(writeBuffer);

    if (ftHandle != NULL) {
        FT_Close(ftHandle);
    }

    return retCode;
}

int fill_bufferWithRandomByte(unsigned char *buff, int DataSize) {
    int id;

    srand (time(NULL));
    for(id=0; id<DataSize; id++) {
        buff[id] = (unsigned char)rand();
    }

    return 0;
}

void fill_engineVariable(dlabTxMsg_t *v, int len, unsigned char *buf) {
    fill_board_message_header(&v->hdr, DST_DLA);    

    memcpy(v->data, buf, len);

    fill_board_message_length(&v->hdr, (int)(sizeof(bdMsgHdr_t) 
                              + MAX_DLAB_TX_MSG_LEN));
}

int do_engine(int portNum, int BaudRate, unsigned char *wb, int ws, 
              unsigned char *rb, int *rs) {
    int l = ws, seg=0;
    unsigned char *buf;
    dlabTxMsg_t tMsg;
    dlabRxMsg_t rMsg;

    buf = wb;
    while(l > 0) {
        memset(&tMsg, 0, sizeof(dlabTxMsg_t));
        if(l >= MAX_DLAB_TX_MSG_LEN) {
            seg = MAX_DLAB_TX_MSG_LEN;
        } else {
            seg = l;
        }

        fill_engineVariable(&tMsg, seg, buf);
        memset(&rMsg, 0, sizeof(dlabRxMsg_t));
        if(transceive_data_with_dla_board(portNum, (char *)&tMsg, 
                          (int)sizeof(dlabTxMsg_t), (char *)&rMsg, 
                          (int)sizeof(dlabRxMsg_t), BaudRate)) {
            lprintf(LOG_ERR, "Fail to transceive_data_with_dla_board.");
            return -1;
        }
        memcpy(rb, rMsg.data, MAX_DLAB_RX_MSG_LEN);

        l -= MAX_DLAB_TX_MSG_LEN;
        *rs += MAX_DLAB_RX_MSG_LEN;
        if(l) {
            buf += MAX_DLAB_TX_MSG_LEN;
            rb  += MAX_DLAB_RX_MSG_LEN;
        }
    }

    return 0;
}

int dla_engine(int portNum, int DataSize, int BaudRate, char *InputFileName) {
    int bytesRead = 0, retCode = -1;                     // Assume failure
    unsigned char *writeBuffer = NULL, *readBuffer = NULL;
    size_t len = DataSize;
    FILE    *rfp;

    lprintf(LOG_DEBUG, 
            "%s(portNum(%d), DataSize(%d), BaudRate(%d), InputFileName:%s))", 
            __func__, portNum, DataSize, BaudRate, InputFileName);
    writeBuffer = (unsigned char *)calloc(DataSize, sizeof(unsigned char));
    if (writeBuffer == NULL) {
        goto exit_engine;
    }
    readBuffer = (unsigned char *)calloc(DataSize, sizeof(unsigned char));
    if (readBuffer == NULL) {
        goto exit_engine;
    }

    if(strlen(InputFileName) == 0) {
        fill_bufferWithRandomByte(writeBuffer, DataSize);
    } else {
        rfp = fopen(InputFileName, "rb");
        if(rfp == NULL) {
            lprintf(LOG_ERR, "Cannot open %s file !", InputFileName);
            goto exit_engine;
        }

        len = fread(writeBuffer, sizeof(unsigned char), DataSize, rfp);
        fclose(rfp);
    }

    if((portNum<0) || (portNum>16)) {
        portNum = 0;
    }
    if (BaudRate < 0) {
        BaudRate = 9600;
    }

    lprintf(LOG_NOTICE, "\nPlaintext:");
    dumpBuffer(writeBuffer, len);
    if(do_engine(portNum, BaudRate, writeBuffer, len, readBuffer, &bytesRead)) {
        lprintf(LOG_ERR, "Fail to do_engine !");
        goto exit_engine;
    }

    lprintf(LOG_NOTICE, "MD5 Result:");
    dumpBuffer(readBuffer, bytesRead);

    retCode = 0;

exit_engine:
    free(readBuffer);
    free(writeBuffer);

    return retCode;
}

int do_aes(int portNum, int BaudRate, unsigned char *wb, int ws, 
           unsigned char *rb, int *rs) {
    aesMsg_t *tx;

    tx = (aesMsg_t *)wb;

    fill_board_message_header(&tx->hdr, DST_DLA);    
    fill_board_message_length(&tx->hdr, ws);

    if(transceive_data_with_dla_board(portNum, (char *)wb, 
                           (int)sizeof(aesMsg_t), (char *)rb, 
                           (int)sizeof(aesMsg_t), BaudRate)) {
        lprintf(LOG_ERR, "Fail to transceive_data_with_dla_board.");
        return -1;
    }

    *rs = sizeof(aesMsg_t);
    return 0;
}

int do_aes_continuous(int portNum, int BaudRate, unsigned char *wb, int ws, 
                      unsigned char *rb, int *rs) {
    aesMsg_t *tx;

    if(write_register(0, 115200, 1, 0x300, 0)) {
        lprintf(LOG_ERR, "Fail to write_register.");
        return -1;
    }

    tx = (aesMsg_t *)wb;

    fill_board_message_header(&tx->hdr, DST_DLA);    
    fill_board_message_length(&tx->hdr, ws);

    if(transfer_data_to_dla_board(portNum, (char *)wb, 
                        (int)sizeof(aesMsg_t), BaudRate)) {
        lprintf(LOG_ERR, "Fail to transfer_data_to_dla_board.");
        return -1;
    }

    if(write_register(0, 115200, 1, 0x300, 1)) {
        lprintf(LOG_ERR, "Fail to write_register.");
        return -1;
    }

    *rs = sizeof(aesMsg_t);
    return 0;
}

int do_aes_stop(int portNum, int BaudRate, unsigned char *wb, int ws, 
                unsigned char *rb, int *rs) {
    cfrbMsg_t tMsg;

    memset(&tMsg, 0, sizeof(cfrbMsg_t));
    fill_registerVariable(&tMsg, 1, 1, 0x300, 0);
    if(transceive_data_with_dla_board(portNum, (char *)&tMsg, 
                           (int)sizeof(cfrbMsg_t), (char *)rb, 
                           (int)sizeof(aesMsg_t), BaudRate)) {
        lprintf(LOG_ERR, "Fail to transceive_data_with_dla_board.");
        return -1;
    }

    *rs = sizeof(aesMsg_t);
    return 0;
}

void printAesResult(aesMsg_t *rMsg) {
    int cVal = 0, i;
    char cipherStr[1024];

    lprintf(LOG_NOTICE, "AES Result:");
    cVal = rMsg->acH & 0xFF;
    cVal = (cVal << 8) + (rMsg->acL & 0xFF);
    lprintf(LOG_NOTICE, "          Count: %d", cVal);

    memset(cipherStr, 0, 1024);
    for(i=0; i<DLAB_AES_KEY_VALUE_LEN; i++) {
        sprintf(cipherStr, "%s 0x%02x", cipherStr, rMsg->key[i] & 0xFF);
    }
    lprintf(LOG_NOTICE, "  cipherText[0]:%s", cipherStr);

    memset(cipherStr, 0, 1024);
    for(i=0; i<DLAB_AES_KEY_VALUE_LEN; i++) {
        sprintf(cipherStr, "%s 0x%02x", cipherStr, rMsg->plain_text[i] & 0xFF);
    }
    lprintf(LOG_NOTICE, "  cipherText[1]:%s", cipherStr);
}

int dla_aes(int portNum, int BaudRate, int count, unsigned char *key, 
            unsigned char *ptext) {
    int bytesRead = 0, retCode = -1, i;                     // Assume failure
    unsigned char *writeBuffer = NULL, *readBuffer = NULL;
    size_t len;
    aesMsg_t tMsg, rMsg;

    lprintf(LOG_DEBUG, 
            "%s(portNum(%d), BaudRate(%d), Count(%d)))", 
            __func__, portNum, BaudRate, count);
    memset(&tMsg, 0, sizeof(aesMsg_t));
    memset(&rMsg, 0, sizeof(aesMsg_t));
    writeBuffer = (unsigned char *)&tMsg;
    readBuffer = (unsigned char *)&rMsg;

    tMsg.acL = count & 0xFF;
    tMsg.acH = (count >> 8) & 0xFF;

    for(i=0; i<DLAB_AES_KEY_VALUE_LEN; i++) {
        tMsg.key[i] = key[i];
    }
    for(i=0; i<DLAB_AES_PLAIN_TEXT_LEN; i++) {
        tMsg.plain_text[i] = ptext[i];
    }

    if((portNum<0) || (portNum>16)) {
        portNum = 0;
    }
    if (BaudRate < 0) {
        BaudRate = 9600;
    }
    len = sizeof(aesMsg_t);

    if(count == -1) {
        if(do_aes_stop(portNum, BaudRate, writeBuffer, len, 
                       readBuffer, &bytesRead)) {
            lprintf(LOG_ERR, "Fail to do_engine !");
            goto exit_engine;
        }
        printAesResult((aesMsg_t *)readBuffer);
    } else if(count) {
        if(do_aes(portNum, BaudRate, writeBuffer, len, readBuffer, 
                  &bytesRead)) {
            lprintf(LOG_ERR, "Fail to do_engine !");
            goto exit_engine;
        }
        printAesResult((aesMsg_t *)readBuffer);
    } else {
        if(do_aes_continuous(portNum, BaudRate, writeBuffer, len, readBuffer, 
                             &bytesRead)) {
            lprintf(LOG_ERR, "Fail to do_engine !");
            goto exit_engine;
        }
    }

    retCode = 0;

exit_engine:

    return retCode;
}

int transceive_data_with_ftHandle(FT_HANDLE  ftHandle, char *wb, int tL, 
                                  char *rb, int rL) {
    DWORD dwBytesWritten, dwBytesRead;
    int err = -1;
    DWORD dwRxSize = 0;
    long int timeout = 5; // seconds

    lprintf(LOG_DEBUG, "Calling FT_Write with this write-buffer:");

    /* Write */
    if(write_buffer_FTDI(ftHandle,wb, (DWORD)tL, &dwBytesWritten )) {
        goto exit_transceive_ftHandle;
    }

    /* Read */
    if(check_readBuffer_FTDI(ftHandle, (DWORD)rL, &dwRxSize, timeout)){
        goto exit_transceive_ftHandle;
    } else {
        lprintf(LOG_DEBUG, 
                "\nGot %d (of %d) bytes.", 
                (int)dwRxSize, (int)dwBytesWritten);
        if(dwRxSize > rL) {
            dwRxSize = rL;
        }
        if(read_buffer_FTDI(ftHandle, (unsigned char *)rb, dwRxSize, 
                            &dwBytesRead)) {
            goto exit_transceive_ftHandle;
        }
        lprintf(LOG_DEBUG, 
                "FT_Read read %d bytes.  Read-buffer is now:", 
                (int)dwBytesRead);
        if (verbose >= 2) {
            dumpBuffer((unsigned char *)rb, (int)dwBytesRead);
        }
        lprintf(LOG_DEBUG, "\n%s passed.", __func__);
        err = 0;
    }

exit_transceive_ftHandle:

    return err;
}

int do_mlp(FT_HANDLE  ftHandle, unsigned char *wb, int ws, unsigned char *rb, 
           int *rs) {
    mlpTxMsg_t *tx;

    tx = (mlpTxMsg_t *)wb;

    fill_board_message_header(&tx->hdr, DST_DLA);    
    fill_board_message_length(&tx->hdr, ws);

    if(transceive_data_with_ftHandle(ftHandle, (char *)wb, 
                                     ws, (char *)rb, 
                                     (int)sizeof(mlpRxMsg_t))) {
        lprintf(LOG_ERR, "Fail to transceive_data_with_dla_board.");
        return -1;
    }

    *rs = sizeof(mlpRxMsg_t);
    return 0;
}

int dla_mlp(int portNum, int BaudRate, char *NodeInputFile, int NodeNum, 
            int TraceNum, char *OutputfilePath) {
    float** plaintext = NULL;
    FILE* rfp = NULL, *wfp = NULL;
    char buf[256], cBuff[256];
    unsigned char *writeBuffer = NULL, *readBuffer = NULL;
    int  retCode = -1, i, bytesRead = 0;
    size_t len = 0;
    time_t cTime;
    struct tm cTm;
    mlpTxMsg_t tMsg;
    mlpRxMsg_t rMsg;
    FT_HANDLE  ftHandle = NULL;

    ftHandle = open_FTDI_Port(portNum, BaudRate);
    if(ftHandle == NULL) {
        lprintf(LOG_ERR, "Error - ftHandle is NULL.");
        goto exit_mlp;
    }

    plaintext = (float**)calloc(TraceNum, sizeof(float*));
    if(plaintext == NULL) {
        goto exit_mlp;
    }
    for (i = 0; i < TraceNum; i++) {
        plaintext[i] = NULL;
    }

    snprintf(buf, 256 * sizeof(char), "%s", NodeInputFile);
    rfp = fopen(buf, "rb");
    if(rfp == NULL) {
        lprintf(LOG_ERR, "Cannot open %s file !", buf);
        goto exit_mlp;
    }
    for (i = 0; i < TraceNum; i++) {
        plaintext[i] = (float*)calloc(NodeNum, sizeof(float));
        if(plaintext[i] == NULL) {
            goto exit_mlp;
        }
    }
    for (i = 0; i < TraceNum; i++) {
        len += fread(plaintext[i], sizeof(float), NodeNum, rfp);
    }
    fclose(rfp);

    cTime = time(NULL);
    cTm = *localtime(&cTime);
    sprintf(cBuff, 
            "MLP-Result-%d-%02d-%02d.%02d%02d%02d.bin",
            cTm.tm_year + 1900, cTm.tm_mon + 1, cTm.tm_mday, cTm.tm_hour, 
            cTm.tm_min, cTm.tm_sec);
    wfp = fopen(cBuff, "wb");
    if(wfp == NULL) {
        lprintf(LOG_ERR, "Cannot open %s file !", cBuff);
        goto exit_mlp;
    }

    if((portNum<0) || (portNum>16)) {
        portNum = 0;
    }
    if (BaudRate < 0) {
        BaudRate = 115200;
    }

    writeBuffer = (unsigned char *)&tMsg;
    readBuffer = (unsigned char *)&rMsg;
    len = sizeof(bdMsgHdr_t) + NodeNum * sizeof(float);
    for (i = 0; i < TraceNum; i++) {
        memset(&tMsg, 0, len);
        memset(&rMsg, 0, sizeof(mlpRxMsg_t));
        memcpy(&tMsg.plain_text[0], plaintext[i], NodeNum * sizeof(float));
        if(do_mlp(ftHandle, writeBuffer, len, readBuffer, &bytesRead)) {
            fclose(wfp);
            goto exit_mlp;
        }
        fwrite(&rMsg.result[0], sizeof(unsigned char), MLP_RESULT_LEN, wfp);

        /*
            20221116 POOKY 
            Insert code to get the waveform data file from the oscilloscope.
        */
    }

    fclose(wfp);
    retCode = 0;

exit_mlp:
    if(plaintext != NULL) {
        for (i = 0; i < TraceNum; i++) {
            if(plaintext[i] != NULL) {
                free(plaintext[i]);
            }
        }
        free(plaintext);
    }
    if (ftHandle != NULL) {
        FT_Close(ftHandle);
    }

    return retCode;
}

void fill_scmbVariable(smcbMsg_t *v, int len, unsigned int *buf) {
    fill_board_message_header(&v->hdr, DST_SMCB);

    memcpy(v->data, buf, len * sizeof(unsigned int));          

    fill_board_message_length(&v->hdr, (int)(sizeof(bdMsgHdr_t) + 
                              len * sizeof(unsigned int)));
} 

int check_Main_config_signal(FT_HANDLE  ftHandle, long int timeout) {
    struct timeval startTime, now, elapsed;
    int v, done = 1;

    gettimeofday(&startTime, NULL);
    while(done) {
        gettimeofday(&now, NULL);
        timersub(&now, &startTime, &elapsed);
        if (elapsed.tv_sec > timeout) {
            // We've waited too long.  Give up.
            printf("\n%s done check - Timed out after %ld seconds\n", 
                   __func__, elapsed.tv_sec);
            return -1;
        }

        if(read_register_with_ftHandle(ftHandle, 0, 0x400, &v)) {
            return -1;
        }
        done = v & 0x1;
    }

    return 0;
}

int check_Main_DONE_signal(FT_HANDLE  ftHandle, long int timeout) {
    struct timeval startTime, now, elapsed;
    int v, done = 0;

    gettimeofday(&startTime, NULL);
    while(done == 0) {
        gettimeofday(&now, NULL);
        timersub(&now, &startTime, &elapsed);
        if (elapsed.tv_sec > timeout) {
            // We've waited too long.  Give up.
            lprintf(LOG_DEBUG, 
                    "\n%s done check - Timed out after %ld seconds", 
                    __func__, elapsed.tv_sec);
            printf("\n%s done check - Timed out after %ld seconds\n", 
                   __func__, elapsed.tv_sec);
            return -1;
        }

        if(read_register_with_ftHandle(ftHandle, 0, 0x401, &v)) {
            return -1;
        }
        done = v & 0x1;
    }

    return 0;
}

    // 1.  Check SMR Length & Data FIFO empty
int check_SMR_LengthData_FifoEmpty(int portNum, int BaudRate) {
    struct timeval startTime, now, elapsed;
    int s1, s2, done = 0;
    long int timeout = 3;

    gettimeofday(&startTime, NULL);
    while(done == 0) {
        gettimeofday(&now, NULL);
        timersub(&now, &startTime, &elapsed);
        if (elapsed.tv_sec > timeout) {
            lprintf(LOG_DEBUG, 
                    "\n%s empty check(s1: 0x%04x, s2: 0x%04x) - Timed out after %ld seconds", 
                    __func__, s1, s2, elapsed.tv_sec);
            return -1;
        }

        if(read_register(portNum, BaudRate, 0, 0x410, &s1)) {
            return -1;
        }
        if(read_register(portNum, BaudRate, 0, 0x411, &s2)) {
            return -1;
        }
        if((s1 & 0x8000) && (s2 & 0x8000)) {
            done = 1;    // 1: Empty
        }
    }
    return 0;
}

int preTransferConfigData(FT_HANDLE  ftHandle, unsigned int *wb, int ws) {
    int l = ws, seg = 0, t_cnt = 0;
    smcbMsg_t msg;
    unsigned int *buf;
    FT_STATUS  ftStatus = FT_OK;
    DWORD bytesToWrite, bytesWritten;

    buf = wb;
    while(l > 0) {
        memset(&msg, 0, sizeof(smcbMsg_t));
        if(l >= MAX_SCMB_DATAWORD_LEN) {
            seg = MAX_SCMB_DATAWORD_LEN;
        } else {
            seg = l;
        }
        fill_scmbVariable(&msg, seg, buf);
        lprintf(LOG_INFO, "transfer_data_to_dla_board %dth word", t_cnt);

        bytesToWrite = (int)(sizeof(bdMsgHdr_t) + seg * sizeof(unsigned int));
        bytesWritten = 0;
        ftStatus = FT_Write(ftHandle, (char *)&msg,
                            bytesToWrite, &bytesWritten);
        if (ftStatus != FT_OK) {
            lprintf(LOG_ERR, "Failure.  FT_Write returned %d", (int)ftStatus);
            return -1;
        }

        if (bytesWritten != bytesToWrite) {
            lprintf(LOG_ERR, 
                    "Failure.  FT_Write wrote %d bytes instead of %d.",
                    (int)bytesWritten, (int)bytesToWrite);
            return -1;
        }

        l -= seg;
        t_cnt += seg;
        if(l) { 
            buf += seg; 
        }
    }
    return 0;
}

int transferConfigData_with_ftHandle(FT_HANDLE  ftHandle, 
                                     unsigned int *wb, int ws) {
    int l = ws, seg = 0, t_cnt = 0;
    int s2;
    struct timeval startTime, now, elapsed;
    smcbMsg_t msg;
    unsigned int *buf;
    long int timeout = 3;
    FT_STATUS  ftStatus = FT_OK;
    DWORD bytesToWrite, bytesWritten;
    float pro_b = 0.0;
    int   pro_p = 0, idx;
    char  buff[100];

    buf = wb;
    while(l > 0) {
        memset(&msg, 0, sizeof(smcbMsg_t));
        if(l >= MAX_SCMB_DATAWORD_LEN) {
            seg = MAX_SCMB_DATAWORD_LEN;
        } else {
            seg = l;
        }
        fill_scmbVariable(&msg, seg, buf);
        if(read_register_with_ftHandle(ftHandle, 0, 0x411, &s2)) {
            return -1;
        }

        gettimeofday(&startTime, NULL);
        while ((s2 & 0x7FFF) >= (32768 - 4*MAX_SCMB_DATAWORD_LEN)) {
            gettimeofday(&now, NULL);
            timersub(&now, &startTime, &elapsed);
            if (elapsed.tv_sec > timeout) {
                lprintf(LOG_DEBUG, 
                        "\n%s prevent FIFO overflow(s2: 0x%04x) - Timed out after %ld seconds", 
                        __func__, s2, elapsed.tv_sec);
                return -1;
            }
            if(read_register_with_ftHandle(ftHandle, 0, 0x411, &s2)) {
                return -1;
            }
        }
        lprintf(LOG_INFO, "transfer_data_to_dla_board %dth word", t_cnt);

        bytesToWrite = (int)(sizeof(bdMsgHdr_t) + seg * sizeof(unsigned int));
        bytesWritten = 0;

        ftStatus = FT_Write(ftHandle,
                            (char *)&msg,
                            bytesToWrite,
                            &bytesWritten);
        if (ftStatus != FT_OK) {
            lprintf(LOG_ERR, "Failure.  FT_Write returned %d", (int)ftStatus);
            return -1;
        }

        if (bytesWritten != bytesToWrite) {
            lprintf(LOG_ERR, 
                    "Failure.  FT_Write wrote %d bytes instead of %d.",
                    (int)bytesWritten, (int)bytesToWrite);
            return -1;
        }

        l -= seg;
        t_cnt += seg;
        if(l) { 
            buf += seg; 
        }
        pro_b = (t_cnt * 100.0)/(ws * 1.0);
        pro_p = (t_cnt * 100)/(ws);
        memset(buff, 0, 100);
        sprintf(buff, "Progress-Bar [");
        for(idx=0; idx < (pro_p/2); idx++) {
            sprintf(buff, "%s%s", buff, "#");
        }
        for(idx=(pro_p/2); idx < 50; idx++) {
            sprintf(buff, "%s%s", buff, " ");
        }
        sprintf(buff, "%s%s (%.2f%%)", buff, "]", pro_b);
        printf("%s\r", buff);
        fflush(stdout);
    }

    memset(buff, 0, 100);
    sprintf(buff, "Progress-Bar [");
    for(idx=0; idx < 50; idx++) {
        sprintf(buff, "%s%s", buff, "#");
    }
    sprintf(buff, "%s%s (%.2f%%)", buff, "]", 100.00);
    printf("%s\n", buff);

    return 0;
}

int transferConfigData(int portNum, int BaudRate, unsigned int *wb, int ws) {
    int l = ws, seg = 0, t_cnt = 0;
    smcbMsg_t msg;
    unsigned int *buf;

    buf = wb;
    while(l > 0) {
        memset(&msg, 0, sizeof(smcbMsg_t));
        if(l >= MAX_SCMB_DATAWORD_LEN) {
            seg = MAX_SCMB_DATAWORD_LEN;
        } else {
            seg = l;
        }
        fill_scmbVariable(&msg, seg, buf);
        lprintf(LOG_INFO, "transfer_data_to_dla_board %dth word", t_cnt);
        if(transfer_data_to_dla_board(portNum, (char *)&msg, 
                    (int)(sizeof(bdMsgHdr_t) + seg * sizeof(unsigned int)),
                    BaudRate)) {
            lprintf(LOG_ERR, "Fail to transfer_data_to_dla_board.");
            return -1;
        }
        l -= seg;
        t_cnt += seg;
        if(l) {
            buf += seg;
        }
    }
    return 0;
}

void check_SMCB_registers(int portNum, int BaudRate) {
    int v;
    if(read_register(portNum, BaudRate, 0, 0x400, &v)) { 
        printf("[Error] Read FPGA[%d] Register[%04x]\n", 0, 0x400);
    } else {
        printf("[Success] Read FPGA[%d] Register[%04x]: 0x%08x\n", 0, 0x400, v);
    }
    if(read_register(portNum, BaudRate, 0, 0x401, &v)) { 
        printf("[Error] Read FPGA[%d] Register[%04x]\n", 0, 0x401);
    } else {
        printf("[Success] Read FPGA[%d] Register[%04x]: 0x%08x\n", 0, 0x401, v);
    }

    if(read_register(portNum, BaudRate, 0, 0x400, &v)) { 
        printf("[Error] Read FPGA[%d] Register[%04x]\n", 0, 0x400);
    } else {
        printf("[Success] Read FPGA[%d] Register[%04x]: 0x%08x\n", 0, 0x400, v);
    }
    if(read_register(portNum, BaudRate, 0, 0x401, &v)) { 
        printf("[Error] Read FPGA[%d] Register[%04x]\n", 0, 0x401);
    } else {
        printf("[Success] Read FPGA[%d] Register[%04x]: 0x%08x\n", 0, 0x401, v);
    }
}

FT_HANDLE open_FTDI_Port(int portNum, int baudRate) {
    FT_STATUS  ftStatus = FT_OK;
    FT_HANDLE  ftHandle = NULL;

    if((portNum<0) || (portNum>16)) {
        portNum = 0;
    }
    if (baudRate < 0) {
        baudRate = 9600;
    }

    ftHandle = open_FTDI(portNum);
    if(ftHandle == NULL) {
        lprintf(LOG_ERR, "Error - ftHandle is NULL.");
        goto exit_open_ftdi;
    }
    ftStatus = FT_ResetDevice(ftHandle);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, 
                "Failure.  FT_ResetDevice returned %d.", 
                (int)ftStatus);
        goto exit_open_ftdi;
    }
    ftStatus = FT_SetBaudRate(ftHandle, (ULONG)baudRate);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, 
                "Failure.  FT_SetBaudRate(%d) returned %d.",
                baudRate, (int)ftStatus);
        goto exit_open_ftdi;
    }
    ftStatus = FT_SetDataCharacteristics(ftHandle, FT_BITS_8, 
                                         FT_STOP_BITS_1, FT_PARITY_NONE);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, 
                "Failure.  FT_SetDataCharacteristics returned %d.", 
                (int)ftStatus);
        goto exit_open_ftdi;
    }
    // Indicate our presence to remote computer
    ftStatus = FT_SetDtr(ftHandle);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, 
                "Failure.  FT_SetDtr returned %d.", 
                (int)ftStatus);
        goto exit_open_ftdi;
    }
    // Flow control is needed for higher baud rates
    ftStatus = FT_SetFlowControl(ftHandle, FT_FLOW_RTS_CTS, 0, 0);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, 
                "Failure.  FT_SetFlowControl returned %d.", 
                (int)ftStatus);
        goto exit_open_ftdi;
    }
    // Assert Request-To-Send to prepare remote computer
    ftStatus = FT_SetRts(ftHandle);
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, 
                "Failure.  FT_SetRts returned %d.", 
                (int)ftStatus);
        goto exit_open_ftdi;
    }
    ftStatus = FT_SetTimeouts(ftHandle, 3000, 3000);  // 3 seconds
    if (ftStatus != FT_OK) {
        lprintf(LOG_ERR, 
                "Failure.  FT_SetTimeouts returned %d", 
                (int)ftStatus);
        goto exit_open_ftdi;
    }
    return ftHandle;

exit_open_ftdi:
    if (ftHandle != NULL) {
        FT_Close(ftHandle);
    }
    ftHandle = NULL;
    return ftHandle;
}

