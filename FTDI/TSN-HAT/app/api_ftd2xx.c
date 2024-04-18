/*
 * Assuming libftd2xx.so is in /usr/local/lib, build with:
 * 
 *     gcc -o bitmode main.c -L. -lftd2xx -Wl,-rpath /usr/local/lib
 * 
 * and run with:
 * 
 *     sudo ./bitmode [port number]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <pcap.h>
#include <unistd.h>
#include <fcntl.h>
#include <version.h>
#include <lib_menu.h>
#include <libcom.h>
#include <log.h>
#include <helper.h>
#include <ftd2xx.h>
#include <dla_functions.h>
#include <dla_board.h>
#include "api_ftd2xx.h"

int main_bitmode(int argc, char *argv[]) {
    DWORD     bytesWritten = 0;
    DWORD     baudRate = 9600;
    FT_STATUS ftStatus = FT_OK;
    FT_HANDLE ftHandle;
    UCHAR     outputData;
    UCHAR     pinStatus;
    int       portNumber;
    
    if (argc > 1) {
        sscanf(argv[1], "%d", &portNumber);
    } else {
        portNumber = 0;
    }
    
    ftStatus = FT_Open(portNumber, &ftHandle);
    if (ftStatus != FT_OK) {
        /* FT_Open can fail if the ftdi_sio module is already loaded. */
        printf("FT_Open(%d) failed (error %d).\n", portNumber, (int)ftStatus);
        printf("Use lsmod to check if ftdi_sio (and usbserial) are present.\n");
        printf("If so, unload them using rmmod, as they conflict with ftd2xx.\n");
        return 1;
    }

    /* Enable bit-bang mode, where 8 UART pins (RX, TX, RTS etc.) become
     * general-purpose I/O pins.
     */
    printf("Selecting asynchronous bit-bang mode.\n");    
    ftStatus = FT_SetBitMode(ftHandle, 
                             0xFF, /* sets all 8 pins as outputs */
                             FT_BITMODE_ASYNC_BITBANG);
    if (ftStatus != FT_OK) {
        printf("FT_SetBitMode failed (error %d).\n", (int)ftStatus);
        goto exit;
    }

    /* In bit-bang mode, setting the baud rate gives a clock rate
     * 16 times higher, e.g. baud = 9600 gives 153600 bytes per second.
     */
    printf("Setting clock rate to %d\n", baudRate * 16);
    ftStatus = FT_SetBaudRate(ftHandle, baudRate);
    if (ftStatus != FT_OK) {
        printf("FT_SetBaudRate failed (error %d).\n", (int)ftStatus);
        goto exit;
    }
    
    /* Use FT_Write to set values of output pins.  Here we set
     * them to alternate low and high (0xAA == 10101010).
     */
    outputData = 0xAA;
    ftStatus = FT_Write(ftHandle, &outputData, 1, &bytesWritten);
    if (ftStatus != FT_OK) {
        printf("FT_Write failed (error %d).\n", (int)ftStatus);
        goto exit;
    }

    /* Despite its name, GetBitMode samples the values of the data pins. */
    ftStatus = FT_GetBitMode(ftHandle, &pinStatus);
    if (ftStatus != FT_OK) {
        printf("FT_GetBitMode failed (error %d).\n", (int)ftStatus);
        goto exit;
    }

    if (pinStatus != outputData) {
        printf("Failure: pin data is %02X, but expected %02X\n", 
               (unsigned int)pinStatus,
               (unsigned int)outputData);
        goto exit;
    }

    printf("Success: pin data is %02X, as expected.\n", 
           (unsigned int)pinStatus);

exit:
    /* Return chip to default (UART) mode. */
    (void)FT_SetBitMode(ftHandle, 
                        0, /* ignored with FT_BITMODE_RESET */
                        FT_BITMODE_RESET);

    (void)FT_Close(ftHandle);
    return 0;
}

extern int verbose;

unsigned int rates[] = {300, 600, 1200, 2400, 4800, 9600,
                        19200, 38400, 57600, 115200, 
                        230400, 460800, 576000, 921600,
                        1500000, 2000000, 3000000};
#define BOARD_LOOPBACK_OPTION_STRING  "s:p:b:f:hv"
int checkValidBaudRate(unsigned int b)
{
    int id;

    for (id = 0; id < (int)(sizeof((rates))/sizeof((rates)[0])); id++) {
        if(b == rates[id]) return 0;
    }
    
    return -1;
}

#define BOARD_REGISTER_OPTION_STRING  "p:b:o:f:a:d:hv"
int process_board_registerCmd(int argc, const char *argv[], 
                              menu_command_t *menu_tbl) {
    int PortNum  = FTD2XX_DEF_PORT_NUM;
    int BaudRate = FTD2XX_DEF_BAUD_RATE;
    int rw = BOARD_DEF_OPERATION;
    int fpga = BOARD_DEF_FPGA;
    int addr = 0x0, data = 0x0;
    int argflag;

    while ((argflag = getopt(argc, (char **)argv, 
                             BOARD_REGISTER_OPTION_STRING)) != -1) {
        switch (argflag) {
        case 'p':
            if (str2int(optarg, &PortNum) != 0) {
                lprintf(LOG_ERR, 
                        "Invalid parameter given or out of range for '-p'.");
                return -1;
            }
            if (PortNum < 0) {
                lprintf(LOG_ERR, "PortNum %d is out of range.", PortNum);
                return -1;
            }
            break;
        case 'b':
            if (str2int(optarg, &BaudRate) != 0) {
                lprintf(LOG_ERR, 
                        "Invalid parameter given or out of range for '-b'.");
                return -1;
            }
            if (checkValidBaudRate(BaudRate)) {
                lprintf(LOG_ERR, "BaudRate %d is invalid.", BaudRate);
                return -1;
            }
            break;
        case 'o':
            if (str2int(optarg, &rw) != 0) {
                lprintf(LOG_ERR, 
                        "Invalid parameter given or out of range for '-o'.");
                return -1;
            }
            if ((rw < 0) || (rw > 2)) {
                lprintf(LOG_ERR, "read/write/list %d is out of range.", rw);
                return -1;
            }
            break;
        case 'f':
            if (str2int(optarg, &fpga) != 0) {
                lprintf(LOG_ERR, 
                        "Invalid parameter given or out of range for '-f'.");
                return -1;
            }
            if ((fpga < 0) || (fpga > 1)) {
                lprintf(LOG_ERR, "fpga %d is out of range.", fpga);
                return -1;
            }
            break;
        case 'd':
            if (str2int(optarg, &data) != 0) {
                lprintf(LOG_ERR, 
                        "Invalid parameter given or out of range for '-d'.");
                return -1;
            }
            if (data < 0) {
                lprintf(LOG_ERR, "data %d is out of range.", data);
                return -1;
            }
            break;
        case 'a':
            if (str2int(optarg, &addr) != 0) {
                lprintf(LOG_ERR, 
                        "Invalid parameter given or out of range for '-a'.");
                return -1;
            }
            if (addr < 0) {
                lprintf(LOG_ERR, "addr %d is out of range.", addr);
                return -1;
            }
            break;
        case 'v':
            log_level_set(++verbose);
            if (verbose == 2) {
                /* add version info to debug output */
                lprintf(LOG_DEBUG, "%s\n", VERSION_STRING);
            }
            break;
        case 'h':
            process_manCmd(argc, argv, menu_tbl, ECHO);
            return 0;
        }
    }

    return dla_register(PortNum, BaudRate, rw, fpga, addr, data);
}

menu_command_t  boardCommand_tbl[] = {
    { "register",     EXECUTION_ATTR,   process_board_registerCmd, \
        "   register -p <port-number> -b <baud-rate> -o <read/write/list> -f <fpga> -a <address> -d <data>", \
        "   Writes or reads data values to or from registers in FPGAs on the board\n"
        "   Or show register list in FPGAs on the board\n"
        "       <port-number> default value: 0\n"
        "         <baud-rate> default value: 115200\n"
        "   <read/write/list> default value: 0 (0: read, 1: write, 2: list)\n"
        "              <fpga> default value: 0 (0: control FPGA, 1: main FPGA)\n"
        "           <address> default value: 0\n"
        "              <data> default value: 0\n"},
    { 0,           EXECUTION_ATTR,   NULL, " ", " "}
};

int process_main_boardCmd(int argc, const char *argv[], 
                          menu_command_t *menu_tbl) {
    const char **pav = NULL;
    int targc;

    pav = argv, targc = argc;
    pav++, targc--;

    return lookup_cmd_tbl(targc, pav, boardCommand_tbl, ECHO);
}


