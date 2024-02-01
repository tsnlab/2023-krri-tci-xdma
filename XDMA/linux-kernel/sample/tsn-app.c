#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>

#include <signal.h>
#include <sched.h>

#include <lib_menu.h>
#include <helper.h>
#include <log.h>
#include <version.h>

#include <error_define.h>

#include "../libxdma/api_xdma.h"
#include "platform_config.h"
#include "xdma_common.h"
#include "buffer_handler.h"

int watchStop = 1;
int rx_thread_run = 1;
int tx_thread_run = 1;
int stats_thread_run = 1;
int parse_thread_run = 1;

int verbose = 0;

/************************** Variable Definitions *****************************/
struct reginfo reg_general[] = {
    {"TSN version", REG_TSN_VERSION},
    {"TSN Configuration", REG_TSN_CONFIG},
    {"TSN Control", REG_TSN_CONTROL},
    {"@sys count", REG_SYS_COUNT_HIGH},
    {"TEMAC status", REG_TEMAC_STATUS},
    {"One Second Count", REG_ONE_SECOND_CNT},
    {"gPTP Slave PPS Offset", REG_GPTP_SLAVE_PPS_OFFSET},
    {"Qbv Slot Status", REG_QBV_SLOT_STATUS},
    {"TASB Minimum starting data count", REG_TASB_MIN_STARTING_DATA_CNT},
    {"TPPB Minimum starting data count", REG_TPPB_MIN_STARTING_DATA_CNT},
    {"", -1}
};

struct reginfo reg_rx[] = {
    {"rx packets", REG_RX_PACKETS},
    {"@rx bytes", REG_RX_BYTES_HIGH},
    {"rx drop packets", REG_RX_DROP_PACKETS},
    {"@rx drop bytes", REG_RX_DROP_BYTES_HIGH},
    {"rx input packet counter", REG_RX_INPUT_PACKET_COUNT},
    {"rx output packet counter", REG_RX_OUTPUT_PACKET_COUNT},
    {"rx buffer full drop packet count", REG_RX_BUFFER_FULL_DROP_PACKET_COUNT},
    {"RPPB FIFO status", REG_RPPB_FIFO_STATUS},
    {"RASB FIFO status", REG_RASB_FIFO_STATUS},
    {"MRIB debug", REG_MRIB_DEBUG},
    {"TEMAC rx statistics", REG_TEMAC_RX_STAT},
    {"", -1}
};

struct reginfo reg_tx[] = {
    {"tx packets", REG_TX_PACKETS},
    {"@tx bytes", REG_TX_BYTES_HIGH},
    {"tx drop packets", REG_TX_DROP_PACKETS},
    {"@tx drop bytes", REG_TX_DROP_BYTES_HIGH},
    {"tx timestamp count", REG_TX_TIMESTAMP_COUNT},
    {"@tx timestamp 1", REG_TX_TIMESTAMP1_HIGH},
    {"@tx timestamp 2", REG_TX_TIMESTAMP2_HIGH},
    {"@tx timestamp 3", REG_TX_TIMESTAMP3_HIGH},
    {"@tx timestamp 4", REG_TX_TIMESTAMP4_HIGH},
    {"tx input packet counter", REG_TX_INPUT_PACKET_COUNT},
    {"tx output packet counter", REG_TX_OUTPUT_PACKET_COUNT},
    {"tx buffer full drop packet count", REG_TX_BUFFER_FULL_DROP_PACKET_COUNT},
    {"TASB FIFO status", REG_TASB_FIFO_STATUS},
    {"TPPB FIFO status", REG_TPPB_FIFO_STATUS},
    {"MTIB debug", REG_MTIB_DEBUG},
    {"TEMAC tx statistics", REG_TEMAC_TX_STAT},
    {"", -1}
};

struct reginfo chan_h2c[] = {
    {"H2C Channel Identifier", H2C_CHANNEL_IDENTIFIER},
    {"H2C Channel Control 1", H2C_CHANNEL_CONTROL1},
    {"H2C Channel Control 2", H2C_CHANNEL_CONTROL2},
    {"H2C Channel Control 3", H2C_CHANNEL_CONTROL3},
    {"H2C Channel Status 1", H2C_CHANNEL_STATUS1},
    {"H2C Channel Status 2", H2C_CHANNEL_STATUS2},
    {"H2C Channel Completed Descriptor Count", H2C_CHANNEL_COMPLETED_DESCRIPTOR_COUNT},
    {"H2C Channel Alignments", H2C_CHANNEL_ALIGNMENTS},
    {"H2C Poll Mode Low Write Back Address", H2C_POLL_MODE_LOW_WRITE_BACK_ADDRESS},
    {"H2C Poll Mode High Write Back Address", H2C_POLL_MODE_HIGH_WRITE_BACK_ADDRESS},
    {"H2C Channel Interrupt Enable Mask 1", H2C_CHANNEL_INTERRUPT_ENABLE_MASK1},
    {"H2C Channel Interrupt Enable Mask 2", H2C_CHANNEL_INTERRUPT_ENABLE_MASK2},
    {"H2C Channel Interrupt Enable Mask 3", H2C_CHANNEL_INTERRUPT_ENABLE_MASK3},
    {"H2C Channel Performance Monitor Control", H2C_CHANNEL_PERFORMANCE_MONITOR_CONTROL},
    {"H2C Channel Performance Cycle Count Low", H2C_CHANNEL_PERFORMANCE_CYCLE_COUNT_L},
    {"H2C Channel Performance Cycle Count High", H2C_CHANNEL_PERFORMANCE_CYCLE_COUNT_H},
    {"H2C Channel Performance Data Count Low", H2C_CHANNEL_PERFORMANCE_DATA_COUNT_L},
    {"H2C Channel Performance Data Count High", H2C_CHANNEL_PERFORMANCE_DATA_COUNT_H},
    {"", -1}
};

struct reginfo chan_c2h[] = {
    {"C2H Channel Identifier", C2H_CHANNEL_IDENTIFIER},
    {"C2H Channel Control 1", C2H_CHANNEL_CONTROL1},
    {"C2H Channel Control 2", C2H_CHANNEL_CONTROL2},
    {"C2H Channel Control 3", C2H_CHANNEL_CONTROL3},
    {"C2H Channel Status 1", C2H_CHANNEL_STATUS1},
    {"C2H Channel Status 2", C2H_CHANNEL_STATUS2},
    {"C2H Channel Completed Descriptor Count", C2H_CHANNEL_COMPLETED_DESCRIPTOR_COUNT},
    {"C2H Channel Alignments", C2H_CHANNEL_ALIGNMENTS},
    {"C2H Poll Mode Low Write Back Address", C2H_POLL_MODE_LOW_WRITE_BACK_ADDRESS},
    {"C2H Poll Mode High Write Back Address", C2H_POLL_MODE_HIGH_WRITE_BACK_ADDRESS},
    {"C2H Channel Interrupt Enable Mask 1", C2H_CHANNEL_INTERRUPT_ENABLE_MASK1},
    {"C2H Channel Interrupt Enable Mask 2", C2H_CHANNEL_INTERRUPT_ENABLE_MASK2},
    {"C2H Channel Interrupt Enable Mask 3", C2H_CHANNEL_INTERRUPT_ENABLE_MASK3},
    {"C2H Channel Performance Monitor Control", C2H_CHANNEL_PERFORMANCE_MONITOR_CONTROL},
    {"C2H Channel Performance Cycle Count Low", C2H_CHANNEL_PERFORMANCE_CYCLE_COUNT_L},
    {"C2H Channel Performance Cycle Count High", C2H_CHANNEL_PERFORMANCE_CYCLE_COUNT_H},
    {"C2H Channel Performance Data Count Low", C2H_CHANNEL_PERFORMANCE_DATA_COUNT_L},
    {"C2H Channel Performance Data Count High", C2H_CHANNEL_PERFORMANCE_DATA_COUNT_H},
    {"", -1}
};

/*****************************************************************************/

void xdma_signal_handler(int sig) {

    printf("\nXDMA-APP is exiting, cause (%d)!!\n", sig);
    tx_thread_run = 0;
    sleep(1);
    rx_thread_run = 0;
    sleep(1);
    parse_thread_run = 0;
    stats_thread_run = 0;
    sleep(1);
    exit(0);
}

void signal_stop_handler() {

    if(watchStop) {
        xdma_signal_handler(2);
    } else {
        watchStop = 1;
    }
}

void register_signal_handler() {

    signal(SIGINT,  signal_stop_handler);
    signal(SIGKILL, xdma_signal_handler);
    signal(SIGQUIT, xdma_signal_handler);
    signal(SIGTERM, xdma_signal_handler);
    signal(SIGTSTP, xdma_signal_handler);
    signal(SIGHUP,  xdma_signal_handler);
    signal(SIGABRT, xdma_signal_handler);
}

int tsn_app(int mode, int DataSize, char *InputFileName) {

    pthread_t tid1, tid2;
    rx_thread_arg_t rx_arg;
    tx_thread_arg_t tx_arg;
#ifdef PLATFORM_DEBUG
    pthread_t tid3;
    stats_thread_arg_t st_arg;
#endif
    pthread_t tid4;
    parse_thread_arg_t par_arg;

    if(initialize_buffer_allocation()) {
        return -1;
    }

    register_signal_handler();

    set_register(REG_TASB_MIN_STARTING_DATA_CNT, 0x800F0000);
    //set_register(REG_TASB_MIN_STARTING_DATA_CNT, 0x80010000);
    set_register(REG_TPPB_MIN_STARTING_DATA_CNT, 0x10);

    memset(&rx_arg, 0, sizeof(rx_thread_arg_t));
    memcpy(rx_arg.devname, DEF_RX_DEVICE_NAME, sizeof(DEF_RX_DEVICE_NAME));
    memcpy(rx_arg.fn, InputFileName, MAX_INPUT_FILE_NAME_SIZE);
    rx_arg.mode = mode;
    rx_arg.size = DataSize;
    pthread_create(&tid1, NULL, receiver_thread, (void *)&rx_arg);
#ifdef __USE_MULTI_CORE__
    cpu_set_t cpuset1;
    CPU_ZERO(&cpuset1);
    CPU_SET(3, &cpuset1);
    pthread_setaffinity_np(tid1, sizeof(cpu_set_t), &cpuset1);
#endif

    memset(&tx_arg, 0, sizeof(tx_thread_arg_t));
    memcpy(tx_arg.devname, DEF_TX_DEVICE_NAME, sizeof(DEF_TX_DEVICE_NAME));
    memcpy(tx_arg.fn, InputFileName, MAX_INPUT_FILE_NAME_SIZE);
    tx_arg.mode = mode;
    tx_arg.size = DataSize;
    pthread_create(&tid2, NULL, sender_thread, (void *)&tx_arg);
#ifdef __USE_MULTI_CORE__
    cpu_set_t cpuset2;
    CPU_ZERO(&cpuset2);
    CPU_SET(5, &cpuset2);
    pthread_setaffinity_np(tid2, sizeof(cpu_set_t), &cpuset2);
#endif

#ifdef PLATFORM_DEBUG
    memset(&st_arg, 0, sizeof(stats_thread_arg_t));
    st_arg.mode = mode;
    pthread_create(&tid3, NULL, stats_thread, (void *)&st_arg);
#ifdef __USE_MULTI_CORE__
    cpu_set_t cpuset3;
    CPU_ZERO(&cpuset3);
    CPU_SET(9, &cpuset3);
    pthread_setaffinity_np(tid3, sizeof(cpu_set_t), &cpuset3);
#endif
#endif

    memset(&par_arg, 0, sizeof(parse_thread_arg_t));
    memcpy(par_arg.devname, DEF_TX_DEVICE_NAME, sizeof(DEF_TX_DEVICE_NAME));
    memcpy(par_arg.fn, InputFileName, MAX_INPUT_FILE_NAME_SIZE);
    par_arg.mode = mode;
    par_arg.size = DataSize;
    pthread_create(&tid4, NULL, parse_thread, (void *)&par_arg);
#ifdef __USE_MULTI_CORE__
    cpu_set_t cpuset4;
    CPU_ZERO(&cpuset4);
    CPU_SET(7, &cpuset4);
    pthread_setaffinity_np(tid4, sizeof(cpu_set_t), &cpuset4);
#endif

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
#ifdef PLATFORM_DEBUG
    pthread_join(tid3, NULL);
#endif
    pthread_join(tid4, NULL);

    buffer_release();

    return 0;
}

int process_main_runCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
int process_main_txCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
int process_main_showCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
int process_main_getCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
int process_main_setCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
int process_main_ipcCmd(int argc, const char *argv[], menu_command_t *menu_tbl);


menu_command_t  mainCommand_tbl[] = {
    { "run",  EXECUTION_ATTR,   process_main_runCmd, \
        "   run -m <mode> -f <file name> -s <size>", \
        "   Run tsn test application with data szie in mode\n"
        "            <mode> default value: 0 (0: tsn, 1: normal, 2: loopback-integrity check, 3: performance)\n"
        "       <file name> default value: ./tests/data/datafile0_4K.bin(Binary file for test)\n"
        "            <size> default value: 1024 (64 ~ 4096)"},
    { "tx",   EXECUTION_ATTR,   process_main_txCmd, \
        "   tx -m <mode> -f <file name> -s <size>", \
        "   Run tx test application with data szie in mode\n"
        "            <mode> default value: 0 (0: tsn, 1: normal, 2: loopback-integrity check, 3: performance)\n"
        "       <file name> default value: ./sample/pint-udp-response-packet.dat(Binary file for test)\n"
        "            <size> default value: 1508 (64 ~ 4096)"},
    {"show",  EXECUTION_ATTR, process_main_showCmd, \
        "   show register [gen, rx, tx]\n" 
        "   show channel [h2c, c2h]\n", \
        "   Show XDMA resource"},
    {"get",   EXECUTION_ATTR, process_main_getCmd, \
        "   get register <addr(Hex)>\n", \
        "   get XDMA resource"},
    {"set",   EXECUTION_ATTR, process_main_setCmd, \
        "   set register <addr(Hex)> <data(Hex)>\n", \
        "   set XDMA resource"},
    {"ipc",   EXECUTION_ATTR, process_main_ipcCmd, \
        "   ipc -m <mode> -o <offset> -c <count> -d <value> \n", \
        "          <mode> default value: 0 (0: rd, 1: wr, 2: test, 3: auto)\n"
        "        <offset> default value: 0 (0 ~ 4092, 4 byte aligned value)\n"
        "         <count> default value: 1 (1 ~ 1024)\n"
        "         <value> default value: 0x95302342(4 byte value, Multiple entries possible, Valid only when writing)\n"
        "   Query or manipulate ipc dpram contents"},
    { 0,           EXECUTION_ATTR,   NULL, " ", " "}
};

#define MAIN_RUN_OPTION_STRING  "m:s:f:hv"
int process_main_runCmd(int argc, const char *argv[],
                            menu_command_t *menu_tbl) {
    int mode  = DEFAULT_RUN_MODE;
    int DataSize = MAX_BUFFER_LENGTH;
    char InputFileName[256] = TEST_DATA_FILE_NAME;
    int argflag;

    while ((argflag = getopt(argc, (char **)argv,
                             MAIN_RUN_OPTION_STRING)) != -1) {
        switch (argflag) {
        case 'm':
            if (str2int(optarg, &mode) != 0) {
                printf("Invalid parameter given or out of range for '-m'.");
                return -1;
            }
            if ((mode < 0) || (mode >= RUN_MODE_CNT)) {
                printf("mode %d is out of range.", mode);
                return -1;
            }
            break;
        case 's':
            if (str2int(optarg, &DataSize) != 0) {
                printf("Invalid parameter given or out of range for '-s'.");
                return -1;
            }
            if ((DataSize < 64) || (DataSize > MAX_BUFFER_LENGTH)) {
                printf("DataSize %d is out of range.", DataSize);
                return -1;
            }
            break;
        case 'f':
            memset(InputFileName, 0, 256);
            strcpy(InputFileName, optarg);
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

    return tsn_app(mode, DataSize, InputFileName);
}

int tx_app(int mode, int DataSize, char *InputFileName) {

    pthread_t tid2;
    tx_thread_arg_t    tx_arg;
    pthread_t tid3;
    stats_thread_arg_t st_arg;

    if(initialize_buffer_allocation()) {
        return -1;
    }

    register_signal_handler();

    memset(&tx_arg, 0, sizeof(tx_thread_arg_t));
    memcpy(tx_arg.devname, DEF_TX_DEVICE_NAME, sizeof(DEF_TX_DEVICE_NAME));
    memcpy(tx_arg.fn, InputFileName, MAX_INPUT_FILE_NAME_SIZE);
    tx_arg.mode = mode;
    tx_arg.size = DataSize;
    pthread_create(&tid2, NULL, tx_thread, (void *)&tx_arg);
#ifdef __USE_MULTI_CORE__
    cpu_set_t cpuset2;
    CPU_ZERO(&cpuset2);
    CPU_SET(5, &cpuset2);
    pthread_setaffinity_np(tid2, sizeof(cpu_set_t), &cpuset2);
#endif

    memset(&st_arg, 0, sizeof(stats_thread_arg_t));
    st_arg.mode = mode;
    pthread_create(&tid3, NULL, stats_thread, (void *)&st_arg);
#ifdef __USE_MULTI_CORE__
    cpu_set_t cpuset3;
    CPU_ZERO(&cpuset3);
    CPU_SET(9, &cpuset3);
    pthread_setaffinity_np(tid3, sizeof(cpu_set_t), &cpuset3);
#endif

    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);

    buffer_release();

    return 0;
}

#define MAIN_TX_OPTION_STRING  "m:s:f:hv"
int process_main_txCmd(int argc, const char *argv[],
                            menu_command_t *menu_tbl) {
    int mode  = RUN_MODE_DEBUG;
    int DataSize = 1508;
    char InputFileName[256] = "./sample/pint-udp-response-packet.dat"; /*TEST_DATA_FILE_NAME;*/
    int argflag;

    while ((argflag = getopt(argc, (char **)argv,
                             MAIN_TX_OPTION_STRING)) != -1) {
        switch (argflag) {
        case 'm':
            if (str2int(optarg, &mode) != 0) {
                printf("Invalid parameter given or out of range for '-m'.");
                return -1;
            }
            if ((mode < 0) || (mode >= RUN_MODE_CNT)) {
                printf("mode %d is out of range.", mode);
                return -1;
            }
            break;
        case 's':
            if (str2int(optarg, &DataSize) != 0) {
                printf("Invalid parameter given or out of range for '-s'.");
                return -1;
            }
            if ((DataSize < 64) || (DataSize > MAX_BUFFER_LENGTH)) {
                printf("DataSize %d is out of range.", DataSize);
                return -1;
            }
            break;
        case 'f':
            memset(InputFileName, 0, 256);
            strcpy(InputFileName, optarg);
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

    return tx_app(mode, DataSize, InputFileName);
}

int32_t fn_show_register_genArgument(int32_t argc, const char *argv[]);
int32_t fn_show_register_rxArgument(int32_t argc, const char *argv[]);
int32_t fn_show_register_txArgument(int32_t argc, const char *argv[]);

argument_list_t  showRegisterArgument_tbl[] = {
        {"gen", fn_show_register_genArgument},
        {"rx",  fn_show_register_rxArgument},
        {"tx",  fn_show_register_txArgument},
        {0,     NULL}
    };

int32_t fn_show_channel_h2cArgument(int32_t argc, const char *argv[]);
int32_t fn_show_channel_c2hArgument(int32_t argc, const char *argv[]);
argument_list_t  showChannelArgument_tbl[] = {
        {"h2c", fn_show_channel_h2cArgument},
        {"c2h", fn_show_channel_c2hArgument},
        {0,     NULL}
    };

int fn_show_registerArgument(int argc, const char *argv[]);
int fn_show_channelArgument(int argc, const char *argv[]);
argument_list_t  showArgument_tbl[] = {
        {"register", fn_show_registerArgument},
        {"channel", fn_show_channelArgument},
        {0,          NULL}
    };

int fn_get_registerArgument(int argc, const char *argv[]);
argument_list_t  getArgument_tbl[] = {
        {"register", fn_get_registerArgument},
        {0,          NULL}
    };

int fn_set_registerArgument(int argc, const char *argv[]);
argument_list_t  setArgument_tbl[] = {
        {"register", fn_set_registerArgument},
        {0,          NULL}
    };

#define XDMA_REGISTER_DEV    "/dev/xdma0_user"
#define XDMA_CHANNEL_DEV     "/dev/xdma0_control"

int set_register(int offset, uint32_t val) {

    return xdma_api_wr_register(XDMA_REGISTER_DEV, offset, 'w', val);
}

uint32_t get_register(int offset) {

    uint32_t read_val = 0;
    xdma_api_rd_register(XDMA_REGISTER_DEV, offset, 'w', &read_val);

    return read_val;
}

uint32_t get_channel(int offset) {

    uint32_t read_val = 0;
    xdma_api_rd_register(XDMA_CHANNEL_DEV, offset, 'w', &read_val);

    return read_val;
}

void dump_reginfo(struct reginfo* reginfo) {

    for (int i = 0; reginfo[i].offset >= 0; i++) {
        if (reginfo[i].name[0] == '@') {
            uint64_t ll = get_register(reginfo[i].offset);
            ll = (ll << 32) | get_register(reginfo[i].offset + 4);
            printf("%s : 0x%lx\n", &(reginfo[i].name[1]), ll);
        } else {
            printf("%s : 0x%x\n", reginfo[i].name, (unsigned int)get_register(reginfo[i].offset));
        }
    }
}

void dump_chaninfo(struct reginfo* reginfo) {

    for (int i = 0; reginfo[i].offset >= 0; i++) {
        if (reginfo[i].name[0] == '@') {
            uint64_t ll = get_channel(reginfo[i].offset);
            ll = (ll << 32) | get_channel(reginfo[i].offset + 4);
            printf("%s : 0x%lx\n", &(reginfo[i].name[1]), ll);
        } else {
            printf("%s : 0x%x\n", reginfo[i].name, (unsigned int)get_channel(reginfo[i].offset));
        }
    }
}

void dump_registers(int dumpflag, int on) {
    printf("==== Register Dump[%d] Start ====\n", on);
    if (dumpflag & DUMPREG_GENERAL) dump_reginfo(reg_general);
    if (dumpflag & DUMPREG_RX) dump_reginfo(reg_rx);
    if (dumpflag & DUMPREG_TX) dump_reginfo(reg_tx);
    if (dumpflag & DUMPREG_H2C) dump_chaninfo(chan_h2c);
    if (dumpflag & DUMPREG_C2H) dump_chaninfo(chan_c2h);
    printf("==== Register Dump[%d] End ====\n", on);
}

sysclock_t get_sys_count() {

    return ((uint64_t)get_register(REG_SYS_COUNT_HIGH) << 32) | get_register(REG_SYS_COUNT_LOW);
}

sysclock_t get_tx_timestamp(int timestamp_id) {

    switch (timestamp_id) {
    case 1:
        return ((uint64_t)get_register(REG_TX_TIMESTAMP1_HIGH) << 32) | get_register(REG_TX_TIMESTAMP1_LOW);
    case 2:
        return ((uint64_t)get_register(REG_TX_TIMESTAMP2_HIGH) << 32) | get_register(REG_TX_TIMESTAMP2_LOW);
    case 3:
        return ((uint64_t)get_register(REG_TX_TIMESTAMP3_HIGH) << 32) | get_register(REG_TX_TIMESTAMP3_LOW);
    case 4:
        return ((uint64_t)get_register(REG_TX_TIMESTAMP4_HIGH) << 32) | get_register(REG_TX_TIMESTAMP4_LOW);
    default:
        return 0;
    }
}

int32_t fn_show_register_genArgument(int32_t argc, const char *argv[]) {

    dump_registers(DUMPREG_GENERAL, 1);
    return 0;
}

int32_t fn_show_register_rxArgument(int32_t argc, const char *argv[]) {

    dump_registers(DUMPREG_RX, 1);
    return 0;
}

int32_t fn_show_register_txArgument(int32_t argc, const char *argv[]) {

    dump_registers(DUMPREG_TX, 1);
    return 0;
}

int fn_show_registerArgument(int argc, const char *argv[]) {

    if(argc <= 0) {
        printf("%s needs a parameter\r\n", __func__);
        return ERR_PARAMETER_MISSED;
    }

    for (int index = 0; showRegisterArgument_tbl[index].name; index++) {
        if (!strcmp(argv[0], showRegisterArgument_tbl[index].name)) {
            showRegisterArgument_tbl[index].fP(argc, argv);
            return 0;
        }
    }

    return ERR_INVALID_PARAMETER;
}

int32_t fn_show_channel_h2cArgument(int32_t argc, const char *argv[]) {

    dump_registers(DUMPREG_H2C, 1);
    return 0;
}

int32_t fn_show_channel_c2hArgument(int32_t argc, const char *argv[]) {

    dump_registers(DUMPREG_C2H, 1);
    return 0;
}

int fn_show_channelArgument(int argc, const char *argv[]) {

    if(argc <= 0) {
        printf("%s needs a parameter\r\n", __func__);
        return ERR_PARAMETER_MISSED;
    }

    for (int index = 0; showChannelArgument_tbl[index].name; index++) {
        if (!strcmp(argv[0], showChannelArgument_tbl[index].name)) {
            showChannelArgument_tbl[index].fP(argc, argv);
            return 0;
        }
    }

    return ERR_INVALID_PARAMETER;
}

int32_t char_to_hex(char c) {
    if ((c >= '0') && (c <= '9')) {
        return (c - 48);
    } else if ((c >= 'A') && (c <= 'F')) {
        return (c - 55);
    } else if ((c >= 'a') && (c <= 'f')) {
        return (c - 87);
    } else {
        return -1;
    }
}

int32_t str_to_hex(char *str, int32_t *n) {
    int32_t i;
    int32_t len;
    int32_t v = 0;

    len = strlen(str);
    for (i = 0; i < len; i++) {
        if (!isxdigit(str[i])) {
            return -1;
        }
        v = v * 16 + char_to_hex(str[i]);
    }

    *n = v;
    return 0;
}

int fn_get_registerArgument(int argc, const char *argv[]) {

    int32_t addr = 0x0010; // Scratch Register
    int32_t value;

    if(argc > 0) {
        if (str_to_hex((char *)argv[0], &addr) != 0) {
            printf("Invalid parameter: %s\r\n", argv[0]);
            return ERR_INVALID_PARAMETER;
        }
    }

    if(addr % 4) {
        printf("The address value(0x%08x) does not align with 4-byte alignment.\r\n", addr);
        return ERR_INVALID_PARAMETER;
    }

    value = get_register(addr);
    printf("address(%08x): %08x\n", addr, value);

    return 0;
}

int fn_set_registerArgument(int argc, const char *argv[]) {

    int32_t addr = 0x0010; // Scratch Register
    int32_t value = 0x95302342;

    if(argc > 0) {
        if (str_to_hex((char *)argv[0], &addr) != 0) {
            printf("Invalid parameter: %s\r\n", argv[0]);
            return ERR_INVALID_PARAMETER;
        }
        if(argc > 1) {
            if (str_to_hex((char *)argv[1], &value) != 0) {
                printf("Invalid parameter: %s\r\n", argv[1]);
                return ERR_INVALID_PARAMETER;
            }
        }
    }

    if(addr % 4) {
        printf("The address value(0x%08x) does not align with 4-byte alignment.\r\n", addr);
        return ERR_INVALID_PARAMETER;
    }

    set_register(addr, value);
    printf("address(%08x): %08x\n", addr, get_register(addr));

    return 0;
}

int process_main_showCmd(int argc, const char *argv[], 
                             menu_command_t *menu_tbl) {

    if(argc <= 1) {
        print_argumentWarningMessage(argc, argv, menu_tbl, NO_ECHO);
        return ERR_PARAMETER_MISSED;
    }
    argv++, argc--;
    for (int index = 0; showArgument_tbl[index].name; index++)
        if (!strcmp(argv[0], showArgument_tbl[index].name)) {
            argv++, argc--;
            showArgument_tbl[index].fP(argc, argv);
            return 0;
        }

    return ERR_INVALID_PARAMETER;
}

int process_main_getCmd(int argc, const char *argv[], 
                             menu_command_t *menu_tbl) {

    if(argc <= 2) {
        print_argumentWarningMessage(argc, argv, menu_tbl, NO_ECHO);
        return ERR_PARAMETER_MISSED;
    }
    argv++, argc--;
    for (int index = 0; getArgument_tbl[index].name; index++)
        if (!strcmp(argv[0], getArgument_tbl[index].name)) {
            argv++, argc--;
            getArgument_tbl[index].fP(argc, argv);
            return 0;
        }

    return ERR_INVALID_PARAMETER;
}

int process_main_setCmd(int argc, const char *argv[], 
                             menu_command_t *menu_tbl) {

    if(argc <= 3) {
        print_argumentWarningMessage(argc, argv, menu_tbl, NO_ECHO);
        return ERR_PARAMETER_MISSED;
    }
    argv++, argc--;
    for (int index = 0; setArgument_tbl[index].name; index++)
        if (!strcmp(argv[0], setArgument_tbl[index].name)) {
            argv++, argc--;
            setArgument_tbl[index].fP(argc, argv);
            return 0;
        }

    return ERR_INVALID_PARAMETER;
}

int read_ipc_data(int offset, int count, uint32_t ipc_data[]) {

    return xdma_api_rd_ipc_data(XDMA_REGISTER_DEV, offset, count, ipc_data);
}

int write_ipc_data(int offset, int count, uint32_t ipc_data[]) {

    return xdma_api_wr_ipc_data(XDMA_REGISTER_DEV, offset, count, ipc_data);
}

int test_ipc_data(int offset, int count, uint32_t ipc_data[]) {

    int result;
    int index;
    uint32_t ipc_rd_data[1024];

    result = xdma_api_wr_ipc_data(XDMA_REGISTER_DEV, offset, count, ipc_data);
    if(result) {
        printf("%s - error, xdma_api_wr_ipc_data\n", __func__);
    }
    result = xdma_api_rd_ipc_data(XDMA_REGISTER_DEV, offset, count, ipc_rd_data);
    if(result) {
        printf("%s - error, xdma_api_rd_ipc_data\n", __func__);
    }

    for(index = 0; index < count; index++) {
        if(ipc_data[index] != ipc_rd_data[index]) {
            printf("wr_ipc_data[%4d]: 0x%08x, rd_ipc_data[%4d]: 0x%08x\n", 
                    index, ipc_data[index], index, ipc_rd_data[index]);
        }
    }

    return result;
}

#define IPC_STATE_READY      0x1
#define IPC_STATE_TRIGER     0x2
#define IPC_STATE_PROCESSING 0x3

int auto_ipc_data(int offset, int count, uint32_t ipc_data[]) {

    int result;
    uint32_t ipc_rd_data[1024];
    uint32_t msg0 = 0x0;
    uint32_t msg1 = 0x100;

    ipc_rd_data[0] = IPC_STATE_READY;
    result = xdma_api_wr_ipc_data(XDMA_REGISTER_DEV, 0x40, 1, ipc_rd_data);
    if(result) {
        printf("%s - error, xdma_api_wr_ipc_data(offset: 0x40)\n", __func__);
        return -1;
    }

    for(int idx = 0 ; idx < count; idx++) {
        // #define IPC_MB_2_XDMA_MSG_OFFSET                    0x0800
        result = xdma_api_rd_ipc_data(XDMA_REGISTER_DEV, 0, 3, ipc_rd_data);
        if(result) {
            printf("%s - error, xdma_api_rd_ipc_data(offset: 0), idx: %d\n", __func__, idx);
            return -1;
        }

        if(ipc_rd_data[0] == IPC_STATE_TRIGER) {
            ipc_rd_data[0] = IPC_STATE_PROCESSING;
            result = xdma_api_wr_ipc_data(XDMA_REGISTER_DEV, 0, 1, ipc_rd_data);
            if(result) {
                printf("%s - error, xdma_api_wr_ipc_data(offset: 0), idx: %d\n", __func__, idx);
                return -1;
            }
            printf("    Received IPC msg. : 0x%08x 0x%08x\n", ipc_rd_data[1], ipc_rd_data[2]);
            ipc_rd_data[0] = IPC_STATE_READY;
            result = xdma_api_wr_ipc_data(XDMA_REGISTER_DEV, 0, 1, ipc_rd_data);
            if(result) {
                printf("%s - error, xdma_api_wr_ipc_data(offset: 0), idx: %d\n", __func__, idx);
                return -1;
            }
        }

        result = xdma_api_rd_ipc_data(XDMA_REGISTER_DEV, 0x40, 1, ipc_rd_data);
        if(result) {
            printf("%s - error, xdma_api_rd_ipc_data(offset: 0x40), idx: %d\n", __func__, idx);
            return -1;
        }
        if(ipc_rd_data[0] == IPC_STATE_READY) {
            ipc_rd_data[0] = msg0;
            ipc_rd_data[1] = msg1;
            msg0++;
            msg1++;
            result = xdma_api_wr_ipc_data(XDMA_REGISTER_DEV, 0x44, 2, ipc_rd_data);
            if(result) {
                printf("%s - error, xdma_api_wr_ipc_data(offset: 44), idx: %d\n", __func__, idx);
                return -1;
            }
            ipc_rd_data[0] = IPC_STATE_TRIGER;
            result = xdma_api_wr_ipc_data(XDMA_REGISTER_DEV, 0x40, 1, ipc_rd_data);
            if(result) {
                printf("%s - error, xdma_api_wr_ipc_data(offset: 40), idx: %d\n", __func__, idx);
                return -1;
            }
        }

        sleep(1);
    }

    return 0;
}

int ipc_app(int mode, int offset, int count, uint32_t ipc_data[]) {

    switch(mode) {
    case 0: // read
        return read_ipc_data(offset, count, ipc_data);
    break;
    case 1: // write
        return write_ipc_data(offset, count, ipc_data);
    break;
    case 2: // test
        return test_ipc_data(offset, count, ipc_data);
    break;
    case 3: // auto
        return auto_ipc_data(offset, count, ipc_data);
    break;
    }

    return 0;
}

#define MAIN_IPC_OPTION_STRING  "m:o:c:d:hv"
int process_main_ipcCmd(int argc, const char *argv[],
                            menu_command_t *menu_tbl) {
    int mode   = 0; // 0: read, 1: write
    int count  = 1;
    int offset = 0;
    unsigned int value = 0;
    uint32_t ipc_data[1024];
    int index = 0;
    int argflag;
    int result;
    int idx;

    while ((argflag = getopt(argc, (char **)argv,
                             MAIN_IPC_OPTION_STRING)) != -1) {
        switch (argflag) {
        case 'm':
            if (str2int(optarg, &mode) != 0) {
                printf("Invalid parameter given or out of range for '-m'.");
                return -1;
            }
            if ((mode < 0) || (mode > 3)) {
                printf("mode %d is out of range.", mode);
                return -1;
            }
            break;
        case 'o':
            if (str2int(optarg, &offset) != 0) {
                printf("Invalid parameter given or out of range for '-o'.");
                return -1;
            }
            if ((offset < 0) || (offset > 4092)) {
                printf("offset %d is out of range.", offset);
                return -1;
            }
            if(offset % 4) {
                printf("The offset value(%d) is not aligned by 4 bytes.", offset);
                return -1;
            }
            break;
        case 'c':
            if (str2int(optarg, &count) != 0) {
                printf("Invalid parameter given or out of range for '-c'.");
                return -1;
            }
            if ((count < 1) || (count > 1024)) {
                printf("count %d is out of range.", count);
                return -1;
            }
            break;
        case 'd':
            if (str2uint(optarg, &value) != 0) {
                printf("Invalid parameter given or out of range for '-d'.");
                return -1;
            }
            ipc_data[index++] = value;
            index %= 1024;
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

    if(mode == 1) { // write
        for(idx = index; idx < count; idx++) {
            ipc_data[idx] = 0x95302342;
        }
    }
    if(mode == 2) { // test
        for(idx = 0; idx < count; idx++) {
            ipc_data[idx] = idx;
        }
    }
    result = ipc_app(mode, offset, count, ipc_data);
    if(result) {
        printf("Failt to ipc_app, result: %d\n", result);
    } else {
        if(mode == 0) { // read
            for(idx = 0; idx < count; idx++) {
                if(((idx) % 8) == 0) {
                    printf("\n");
                }
                printf(" %08x", ipc_data[idx]);
            }
            printf("\n");
        }
    }

    return result;
}

int command_parser(int argc, char ** argv) {
    char **pav = NULL;
    int  id;

    for(id=0; id<argc; id++) {
        debug_printf("argv[%d] : %s", id, argv[id]);
    }

    pav = argv;

    return lookup_cmd_tbl(argc, (const char **)pav, mainCommand_tbl, ECHO);
}

int main(int argc, char *argv[]) {
    int id;
    int rc=0, t_argc;
    char **pav = NULL;

    for(id=0; id<argc; id++) {
        debug_printf("argv[%d] : %s\n", id, argv[id]);
    }
    debug_printf("\n");

    t_argc = argc, pav = argv;
    pav++, t_argc--;

    rc = command_parser(t_argc, pav);

    return rc;
}

