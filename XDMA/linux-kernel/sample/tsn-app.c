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

#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>

#include "../libxdma/api_xdma.h"
#include "../libxdma/ioctl_xdma.h"
#include "platform_config.h"
#include "xdma_common.h"
#include "buffer_handler.h"
#include "ethernet.h"

int watchStop = 1;
int rx_thread_run = 1;
int tx_thread_run = 1;
int stats_thread_run = 1;

int verbose = 0;

/************************** Variable Definitions *****************************/
struct reginfo reg_general[] = {
    {"TSN version", REG_TSN_VERSION},
    {"TSN Configuration", REG_TSN_CONFIG},
    {"TSN Control", REG_TSN_CONTROL},
    {"@sys count", REG_SYS_COUNT_HIGH},
    {"TEMAC status", REG_TEMAC_STATUS},
    {"Qbv Slot Status Register", REG_QBV_SLOT_STATUS},
    {"Pulse_at MSB", REG_PULSE_AT_MSB},
    {"Pulse_at LSB", REG_PULSE_AT_LSB},
    {"Cycle_1s", REG_CYCLE_1S},
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
#ifdef ONE_QUEUE_TSN
    {"Rx Debug Register", REG_RX_DEBUG},
#else
    {"MRIB debug", REG_MRIB_DEBUG},
#endif
    {"TEMAC rx statistics", REG_TEMAC_RX_STAT},
    {"TEMAC FCS error count", REG_TEMAC_FCS_COUNT},
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
#ifdef ONE_QUEUE_TSN
    {"Tx PCIe AXIS FIFO Status1 Register", REG_TX_AXIS_FIFO_STATUS1},
    {"Tx TMAC AXIS FIFO Status Register", REG_TX_AXIS_FIFO_STATUS},
    {"Tx AXIS Buffer Status Register", REG_TX_AXIS_BUFFER_STATUS},
    {"Tx back pressure event count Register", REG_TX_BACK_PRESSURE_EVENT_COUNT},
    {"Tx Debug Register", REG_TX_DEBUG},
    {"normal timeout count Register", REG_NORMAL_TIMEOUT_COUNT},
    {"to overflow popped count Register", REG_TO_OVERFLOW_POPPED_COUNT},
    {"to overflow timeout count Register", REG_TO_OVERFLOW_TIMEOUT_COUNT},
    {"timeout_drop_from tick Register", REG_TIMEOUT_DROP_FROM},
    {"timeout_drop_to tick Register", REG_TIMEOUT_DROP_TO},
    {"timeout_drop_sys tick Register", REG_TIMEOUT_DROP_SYS},
#else
    {"TASB FIFO status", REG_TASB_FIFO_STATUS},
    {"TPPB FIFO status", REG_TPPB_FIFO_STATUS},
    {"MTIB debug", REG_MTIB_DEBUG},
#endif
    {"TEMAC tx statistics", REG_TEMAC_TX_STAT},
#ifdef ONE_QUEUE_TSN
    {"tx_not_send_packets", REG_TX_FAIL_PACKETS},
    {"@tx_not_send_bytes", REG_TX_FAIL_BYTES_MSB},
    {"tx_delay_packets", REG_TX_DELAY_PACKETS},
    {"@tx_delay_bytes", REG_TX_DELAY_BYTES_MSB},
#endif
    {"", -1}
};

/*****************************************************************************/

void xdma_signal_handler(int sig) {

    printf("\nXDMA-APP is exiting, cause (%d)!!\n", sig);
    tx_thread_run = 0;
    sleep(1);
    rx_thread_run = 0;
    sleep(1);
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
    rx_thread_arg_t    rx_arg;
    tx_thread_arg_t    tx_arg;
#ifdef PLATFORM_DEBUG
    pthread_t tid3;
    stats_thread_arg_t st_arg;
#endif

    if(initialize_buffer_allocation()) {
        return -1;
    }

    register_signal_handler();

    memset(&rx_arg, 0, sizeof(rx_thread_arg_t));
    memcpy(rx_arg.devname, DEF_RX_DEVICE_NAME, sizeof(DEF_RX_DEVICE_NAME));
    memcpy(rx_arg.fn, InputFileName, MAX_INPUT_FILE_NAME_SIZE);
    rx_arg.mode = mode;
    rx_arg.size = DataSize;
    pthread_create(&tid1, NULL, receiver_thread, (void *)&rx_arg);
    sleep(1);

    memset(&tx_arg, 0, sizeof(tx_thread_arg_t));
    memcpy(tx_arg.devname, DEF_TX_DEVICE_NAME, sizeof(DEF_TX_DEVICE_NAME));
    memcpy(tx_arg.fn, InputFileName, MAX_INPUT_FILE_NAME_SIZE);
    tx_arg.mode = mode;
    tx_arg.size = DataSize;
    pthread_create(&tid2, NULL, sender_thread, (void *)&tx_arg);
    sleep(1);

#ifdef PLATFORM_DEBUG
    memset(&st_arg, 0, sizeof(stats_thread_arg_t));
    st_arg.mode = mode;
    pthread_create(&tid3, NULL, stats_thread, (void *)&st_arg);
#endif

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
#ifdef PLATFORM_DEBUG
    pthread_join(tid3, NULL);
#endif

    buffer_release();

    return 0;
}


int process_main_runCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
int process_main_ioctlCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
int process_main_showCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
int process_main_setCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
#ifdef ONE_QUEUE_TSN
int process_main_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
int process_main_sendCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
#endif

/******************************************************************************
 *                                                                            *
 *                            Function Prototypes                             *
 *                                                                            *
 ******************************************************************************/
int process_main_tx_timestamp_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
int process_main_fpga_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
int process_main_xdma_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
int process_main_tx_tstamp_replica_fpga_logic_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
int tx_timestamp_test_app(int data_size);
int fpga_test_app(void);
int xdma_rx_test_app(void);
int tx_tstamp_replica_fpga_logic_test_app(void);

/******************************************************************************
 *                                                                            *
 *                        Main Command Table (modified)                       *
 *                                                                            *
 ******************************************************************************/
menu_command_t  mainCommand_tbl[] = {
    { "run",   EXECUTION_ATTR,   process_main_runCmd, \
        "   run -m <mode> -f <file name> -s <size>", \
        "   Run tsn test application with data szie in mode\n"
        "            <mode> default value: 0 (0: tsn, 1: normal, 2: loopback-integrity check, 3: performance)\n"
        "       <file name> default value: ./tests/data/datafile0_4K.bin(Binary file for test)\n"
        "            <size> default value: 1024 (64 ~ 4096)"},
    {"show",   EXECUTION_ATTR, process_main_showCmd, \
        "   show register [gen, rx, tx, h2c, c2h, irq, con, h2cs, c2hs, com, msix]\n", \
        "   Show XDMA resource"},
    {"set",    EXECUTION_ATTR, process_main_setCmd, \
        "   set register [gen, rx, tx, h2c, c2h, irq, con, h2cs, c2hs, com, msix] <addr(Hex)> <data(Hex)>\n", \
        "   set XDMA resource"},
    {"ts_test", EXECUTION_ATTR, process_main_tx_timestamp_testCmd, \
        "   ts_test -s <size>", \
        "   This option was created for the reproduction of the Tx timestamp error issue. (Debugging Purpose)\n"},
    {"fpga_test", EXECUTION_ATTR, process_main_fpga_testCmd, \
        "   fpga_test", \
        "   This option was created for Register Read of FPGA\n"},
    {"xdma_rx_test", EXECUTION_ATTR, process_main_xdma_testCmd, \
    "   xdma_rx_test", \
    "   This option was created for Test of xdma rx\n"},
    {"tx_tstamp_replica_fpga_logic_test", EXECUTION_ATTR, process_main_tx_tstamp_replica_fpga_logic_testCmd, \
    "   tx_tstamp replica fpga logic test", \
    "   This option was created for Test of XDMA's Read Operation Reliability\n"},
#ifdef ONE_QUEUE_TSN
    {"test",   EXECUTION_ATTR, process_main_testCmd, \
        "   test register -c <count>\n", \
        "   test FPGA register read/write"},
    { "send",  EXECUTION_ATTR,   process_main_sendCmd, \
        "   send -f <from_tick> -m <margin>", \
        "   Send a test packet\n"
        "       <from_tick> default value: 12500\n"
        "          <margin> default value: 5000"},
#endif
    { 0,           EXECUTION_ATTR,   NULL, " ", " "}
};

/******************************************************************************
 *                                                                            *
 *                     Tx Timestamp Test Command Function                     *
 *                                                                            *
 ******************************************************************************/
/*
 *   This function is created for the reproduction of the Tx timestamp error issue.(Debugging Purpose)
 *   by Ganghyeok Lim (2024.10.07)
 */
#define TEST_RUN_OPTION_STRING  "s:"

int process_main_tx_timestamp_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl)
{
    int data_size = 1024;
    int arg_flag;

    while((arg_flag = getopt(argc, (char**)argv, TEST_RUN_OPTION_STRING)) != -1)
    {
        switch(arg_flag)
        {
            case 's' :
                if(str2int(optarg, &data_size) != 0)
                {
                    printf("Invalid parameter given or out of range for '-s'.");
                    return -1;
                }
                if ((data_size < 64) || (data_size > MAX_BUFFER_LENGTH))
                {
                    printf("Data size %d is out of range.", data_size);
                    return -1;
                }

                break;
        }
    }

    return tx_timestamp_test_app(data_size);
}


/******************************************************************************
 *                                                                            *
 *                  Tx Timestamp Test Application Function                    *
 *                                                                            *
 ******************************************************************************/
/* User configurable Macro definition */
#define TEST_INTERVAL_MS            (INTERVAL_10MS)
#define TOLERANCE_PERCENT           (100)

/* Macro definition */
#define MY_BUFFER_ALIGNMENT         (512)
#define INTERVAL_1000MS             (1000UL)
#define INTERVAL_100MS              (100UL)
#define INTERVAL_10MS               (10UL)
#define TICK_1000MSEC               (125000000UL)
#define TICK_100MSEC                (12500000UL)
#define TICK_10MSEC                 (1250000UL)
#define NUM_ERROR_LOG               (1000UL)
#define FLAG_SET                    (1)
#define FLAG_CLEAR                  (0)

/* Structure for Tx argument */
typedef struct my_tx_arg {
    char devname[MAX_DEVICE_NAME];
    int size;
} my_tx_arg_t;


/* Global variables for Tx  */
char*           my_buffer;              // Buffer used for Packet transmission
int             my_tx_xdma_fd;          // File descriptor for XDMA
my_tx_arg_t     my_tx_arg_data;         // Tx argument structure (XDMA Device name, size of data to Tx)
stats_t         my_tx_stats;            // Tx Statistics structure
uint64_t        tx_length;              // Data size to Tx (Metadata + Ethernet Frame)
uint64_t        tx_index = 1;           // Index of Transmission
uint64_t        bytes_tr;               // Transmitted Byte for each XDMA write
int             xdma_write_status;      // Status of XDMA write api

/* Global variables for Syscount & Tx Timestamp  */
uint64_t        my_syscount, my_syscount_prv;                                   // Syscount of current & previous
uint64_t        my_tx_timestamp, my_tx_timestamp_prv;                           // Tx Timestamp of current & previous
uint64_t        diff_syscount, diff_tx_timestamp, diff_syscount_txtimestamp;    // Difference of Syscount & Tx Timestamp
uint64_t        time_interval_ms = TEST_INTERVAL_MS;                            // Transmission time interval (unit : ms)
uint64_t        ideal_diff;                                                     // Ideal difference value for selected Time interval
uint64_t        ten_percent_of_ideal_diff;                                      // 10% value of Ideal difference value
uint64_t        tx_timestamp_diff_allowed_min, tx_timestamp_diff_allowed_max; // Allowed min/max value of Tx Timestamp Difference

/* Global variables for Error Information */
uint8_t         error_flag = 0;
uint8_t         error_type = 0;
uint64_t        error_count = 0;                        // The number of count that Error occurs
FILE*           error_log_fd;                           // File descriptor for error log text file

// Raspberry pi 5 System time
time_t          my_time;
struct tm       tm_now;
struct tm       tm_prv;

/* Microsecond Time Count */
struct timeval  tv;
double          rasp_time, rasp_time_prv;
double          rasp_time_diff;


int tx_timestamp_test_app(int data_size)
{
    // 1. Initialize structures
    memset(&my_tx_arg_data, 0, sizeof(my_tx_arg_t));
    memset(&my_tx_stats,    0, sizeof(stats_t));
    memset(&tm_now, 0, sizeof(tm_now));
    memset(&tm_prv, 0, sizeof(tm_prv));

    // 2. Register signal handler
    register_signal_handler();

    // 3. Enable TEMAC & XDMA
    set_register(REG_TSN_CONTROL, 1);

    // 4. Open Text file for data logging
    error_log_fd = fopen("Tx Timestamp Error Log.txt", "w");
    if(error_log_fd == NULL)
    {
        printf("Cannot Open error log file!\n");

        return -1;
    }

    fprintf(error_log_fd, "==================== Tx Timestamp Error Log ====================\n");
    fprintf(error_log_fd, "Packet transmission interval : %ld[ms]\n", time_interval_ms);
    my_time = time(NULL);
    tm_now = *localtime(&my_time);
    fprintf(error_log_fd, "Test start time : %d-%d-%d %d:%d:%d\n", tm_now.tm_year+1900, tm_now.tm_mon+1, tm_now.tm_mday, tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
    
    // 5. Allocate buffer for Tx (data_size * 2 [Byte])
    if(posix_memalign((void **)&my_buffer, MY_BUFFER_ALIGNMENT /*alignment : 64 Bytes*/, (data_size*2)))
    {
        printf("Buffer allocation : Failed\n");
        return -1;
    }
    else
    {
        printf("Buffer allocation : Success\n");
        printf("My buffer address : %p\n", my_buffer);
        printf("Buffer Mem align : %d\n", MY_BUFFER_ALIGNMENT);
        fprintf(error_log_fd, "Buffer allocation : Success\n");
        fprintf(error_log_fd, "My buffer address : %p\n", my_buffer);
        fprintf(error_log_fd, "Buffer Mem align : %d\n", MY_BUFFER_ALIGNMENT);
        memset(my_buffer, 0, data_size);
    }
    
    // 6. Config XDMA Device name & Data size from user
    memcpy(my_tx_arg_data.devname, DEF_TX_DEVICE_NAME, sizeof(DEF_TX_DEVICE_NAME));
    my_tx_arg_data.size = data_size;
    printf("XDMA Device name : %s, Data Size : %d\n", my_tx_arg_data.devname, my_tx_arg_data.size);
    printf(error_log_fd, "XDMA Device name : %s, Data Size : %d\n", my_tx_arg_data.devname, my_tx_arg_data.size);

    // 7. Specify start address of each part (metadata, frame, ethernet header)
    struct tsn_tx_buffer* my_tx_buffer  = (struct tsn_tx_buffer*)my_buffer;
    struct tx_metadata* my_tx_metadata  = &my_tx_buffer->metadata;
    uint8_t* my_tx_frame                = (uint8_t*)&my_tx_buffer->data;
    struct ethernet_header* my_tx_eth   = (struct ethernet_header*)my_tx_frame;

    // 8. Config Timestamp id & Ethernet frame length
    my_tx_metadata->timestamp_id    = 1;
    my_tx_metadata->fail_policy     = 0;
    my_tx_metadata->frame_length    = my_tx_arg_data.size;
    my_tx_metadata->from.tick       = (uint32_t)(0);            // from : 0, to : 0x1FFFFFFF => Packet transmission is always possible (at any time)
    my_tx_metadata->to.tick         = (uint32_t)(0x1FFFFFFF);

    // 9. Config Source/Destination MAC
    static const char* myMAC    = "\x00\x11\x22\x95\x30\x23";   // My MAC address
    static const char* desMAC   = "\xFF\xFF\xFF\xFF\xFF\xFF";   // Broadcast MAC address
    memcpy(&(my_tx_eth->dmac), desMAC, 6);
    memcpy(&(my_tx_eth->smac), myMAC, 6);

    // 10. Config Data length to Tx
    tx_length = sizeof(my_tx_buffer->metadata) + my_tx_buffer->metadata.frame_length;

    // 11. Determine Ideal difference value & 10% of ideal difference value for given Time interval
    if(time_interval_ms      == INTERVAL_1000MS) ideal_diff = TICK_1000MSEC;        // When sleep time is 1000[ms]
    else if(time_interval_ms == INTERVAL_100MS)  ideal_diff = TICK_100MSEC;         // When sleep time is 100[ms]
    else if(time_interval_ms == INTERVAL_10MS)   ideal_diff = TICK_10MSEC;          // When sleep time is 10[ms]

    ten_percent_of_ideal_diff = ideal_diff * ((double)TOLERANCE_PERCENT / 100.);    // Allowed tolerance is 10% of ideal difference (between current & previous Tx Timestamp value)
    tx_timestamp_diff_allowed_min  = ideal_diff;                                    // Allowed minimum value of Tx Timestamp
    tx_timestamp_diff_allowed_max  = ideal_diff + ten_percent_of_ideal_diff;        // Allowed maximum value of Tx Timestamp

    // 12. Open XDMA Device
    if(xdma_api_dev_open(my_tx_arg_data.devname, 0 /* eop_flush */, &my_tx_xdma_fd)) {
        printf("FAILURE: Could not open %s. Make sure xdma device driver is loaded and you have access rights (maybe use sudo?).\n", my_tx_arg_data.devname);
        printf("<<< %s\n", __func__);
        fprintf(error_log_fd, "Open XDMA : Failed\n\n");
        return NULL;
    }
    else
    {
        printf("Open XDMA : Success\n\n");
        fprintf(error_log_fd, "Open XDMA : Success\n\n");
    }


    while(1)
    {
        // 13-1. Show Transmission Index
        printf("%ldth Tx ", tx_index);

        // 13-2. Do XDMA Write
        xdma_write_status = xdma_api_write_from_buffer_with_fd(my_tx_arg_data.devname, my_tx_xdma_fd, (char *)my_tx_buffer, tx_length, &bytes_tr);
        if(xdma_write_status == 0)
        {
            // printf("XDMA Write : Success\n");
        }
        else
        {
            printf("XDMA Write : Failed\n");
        }

        // 13-3. Get System Count & Tx Timestamp
        my_syscount     = get_sys_count();
        my_tx_timestamp = get_tx_timestamp(1);

        // 13-4. Increase Statistics (Tx packets, Bytes)
        my_tx_stats.txPackets++;
        my_tx_stats.txBytes += bytes_tr;

        // 13-5. Get Current Raspberry PI5 Time
        my_time = time(NULL);
        tm_now  = *localtime(&my_time);

        // 13-6. Calculate diff of Raspberry PI5 Time
        gettimeofday(&tv, NULL);
        rasp_time = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;   // unit : [ms]
        rasp_time_diff = rasp_time - rasp_time_prv;             // unit : [ms]

        // 13-6. Calculate diff of System count & Tx Timestamp
        if(tx_index > 1)
        {
            diff_syscount               = my_syscount - my_syscount_prv;
            diff_tx_timestamp           = my_tx_timestamp - my_tx_timestamp_prv;
            diff_syscount_txtimestamp   = my_syscount - my_tx_timestamp;
        } 

        // 13-7. Configure Error Condition
        if(my_tx_timestamp == my_tx_timestamp_prv)
        {
            // Error Condition 1 : Current & Previous Tx Timestamp is equal
            error_flag = FLAG_SET;
        }
        else if(my_tx_timestamp < my_tx_timestamp_prv)
        {
            // Error Condition 2 : Current Tx Timestamp is less than Previous
            error_flag = FLAG_SET;
        }
        else if((diff_tx_timestamp < tx_timestamp_diff_allowed_min) || (diff_tx_timestamp > tx_timestamp_diff_allowed_max))
        {
            // Error Condition 3 : Tx Timestamp Difference is out of Allowed range
            error_flag = FLAG_SET;
        }
        else if(diff_syscount_txtimestamp > (uint64_t)0xFFFFF)
        {
            // Error Condition 4 : Current Syscount is 0xFFFFF value more than Tx Timestamp
            error_flag = FLAG_SET;
        }
        else
        {
            // No Error
            error_flag = FLAG_CLEAR;
        }

        // 13-8. If Error occurs, Save Error log to Text File
        if(error_flag == FLAG_SET)
        {
            if(tx_index > 1)
            {
                fprintf(error_log_fd, \
                "\n============================= %ldth Error =============================\
                \nRaspberry PI5 OS Time                       : %d-%d-%d %d:%d:%d\
                \nMeasured Transmission Time Gap              : %.1lf[ms]\
                \n\nTransmission Index                          : %ld\
                \nTransmitted packet                          : %lld\
                \n\nsyscount                   (hex)            : %016lx                |  tx_timestamp      (hex) : %016lx\
                \nsyscount_prv               (hex)            : %016lx                |  tx_timestamp_prv  (hex) : %016lx\
                \n\nsyscount_diff              (dec)            : %16ld  (%.4lf[s])   |  tx_timestamp_diff (dec) : %16ld (%.4lf[s])\
                \nsyscount_diff              (hex)            : %16lx  (%.4lf[s])   |  tx_timestamp_diff (hex) : %16lx (%.4lf[s])\
                \n\nsyscount_txtimestamp_diff  (hex)            : %16lx\
                \nsyscount_txtimestamp_diff  (dec)            : %16ld\
                \n", (error_count+1),\
                tm_now.tm_year+1900, tm_now.tm_mon+1, tm_now.tm_mday, tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec,\
                rasp_time_diff,\
                tx_index, my_tx_stats.txPackets,\
                my_syscount, my_tx_timestamp, my_syscount_prv, my_tx_timestamp_prv,\
                diff_syscount, (double)diff_syscount/TICK_1000MSEC, diff_tx_timestamp, (double)diff_tx_timestamp/TICK_1000MSEC,\
                diff_syscount, (double)diff_syscount/TICK_1000MSEC, diff_tx_timestamp, (double)diff_tx_timestamp/TICK_1000MSEC,\
                diff_syscount_txtimestamp, diff_syscount_txtimestamp);

                fprintf(error_log_fd, \
                "\nIdeal Diff                                  : %ld           (=> %.4lf[s])\
                \nAllowed Tx Timestamp Diff Range (%d%% tor.)  : %ld ~ %ld (=> %.4lf[s])\n\n", ideal_diff, (double)ideal_diff/TICK_1000MSEC, TOLERANCE_PERCENT, tx_timestamp_diff_allowed_min, tx_timestamp_diff_allowed_max, (double)tx_timestamp_diff_allowed_max/TICK_1000MSEC);
                
                error_count++;
            }
        }

        printf("=> total error count : %ld\
        \nsyscount                  (hex) : %016lx               |   tx_timestamp     (hex) : %016lx\
        \nsyscount_prv              (hex) : %016lx               |   tx_timestamp_prv (hex) : %016lx\
        \nsyscount_diff             (dec) : %16ld (%.4lf[s])   |   tx_timestamp_diff(dec) : %16ld (%.4lf[s])\
        \n\nsyscount_txtimestamp_diff (hex) : %16lx                   (Dec : %16ld)\
        \n\n", error_count, my_syscount, my_tx_timestamp, my_syscount_prv, my_tx_timestamp_prv, diff_syscount, (double)diff_syscount/TICK_1000MSEC, diff_tx_timestamp, (double)diff_tx_timestamp/TICK_1000MSEC,\
        diff_syscount_txtimestamp, diff_syscount_txtimestamp);

        // 13-9. Initialize variable for Next transmission
        my_syscount_prv     = my_syscount;
        my_tx_timestamp_prv = my_tx_timestamp;
        rasp_time_prv       = rasp_time;
        bytes_tr = 0;
        memcpy(&tm_prv, &tm_now, sizeof(tm_prv));
        tx_index++;

        // 13-9. Sleep
        usleep(time_interval_ms*1000);
    }

    // 14. Close XDMA device
    close(my_tx_xdma_fd);

    // 15. Disable TEMAC & XDMA
    set_register(REG_TSN_CONTROL, 0);

    // 16. Free buffer
    free(my_buffer);

    // 17. Close Text file for logging data
    fclose(error_log_fd);

    return 0;
}


/******************************************************************************
 *                                                                            *
 *                        FPGA Test Application Function                      *
 *                                                                            *
 ******************************************************************************/
int fpga_test_app(void)
{
    uint32_t    my_32bit;
    uint32_t    hour, minute, second;
    uint32_t    test_write_data = 0;
    
    while(1)
    {
        hour = get_register(REG_TIME_HOUR);
        minute = get_register(REG_TIME_MINUTE);
        second = get_register(REG_TIME_SECOND);

        printf("============= Counter & Clock =============\n");
        printf("// FPGA Run Time => %02ld : %02ld : %02ld\n\n", hour, minute, second);

        printf("// Up Counter\n");
        my_32bit = get_register(REG_UP_COUNTER_HIGH);
        printf("REG_UP_COUNTER_HIGH   : %010ld\n", my_32bit);
        my_32bit = get_register(REG_UP_COUNTER_LOW);
        printf("REG_UP_COUNTER_LOW    : %010ld\n\n", my_32bit);

        printf("// Down Counter\n");
        my_32bit = get_register(REG_DN_COUNTER_HIGH);
        printf("REG_DOWN_COUNTER_HIGH : %010ld\n", my_32bit);
        my_32bit = get_register(REG_DN_COUNTER_LOW);
        printf("REG_DOWN_COUNTER_LOW  : %010ld\n\n", my_32bit);

        for(int i = 0; i < 8; i++)
        {
            set_register(REG_SCRATCH1 + 4*i, test_write_data);
            usleep(10);
            my_32bit = get_register(REG_SCRATCH1 + 4*i);
            printf("Data in Scratch Register%d : %010ld\n", (i+1), my_32bit);
        }

        printf("\n\n");

        test_write_data += 10;

        usleep(100*1000);
    }

    return 0;
}


/******************************************************************************
 *                                                                            *
 *                      XDMA RX Test Application Function                     *
 *                                                                            *
 ******************************************************************************/
#define BUF_ALIGN       (16)     // 16Byte Alignment

int xdma_rx_test_app(void)
{
    char my_devname[MAX_DEVICE_NAME];
    int my_rx_xdma_fd;
    int data_size = 16;     // 16 Byte (= 128bit)
    int bytes_rcv = 0;

    // 1. Register signal handler
    register_signal_handler();

    // 2. Enable TEMAC & XDMA
    set_register(REG_TSN_CONTROL, 1);

    // 3. Allocate buffer for Rx (Size : 16Byte)
    if(posix_memalign((void **)&my_buffer, BUF_ALIGN /* alignment : 16 Bytes */, data_size /* data_size : 16 Bytes (= 128bits) */))
    {
        printf("Buffer allocation : Failed\n");
        return -1;
    }
    else
    {
        printf("Buffer allocation : Success\n");
        printf("My buffer address : %p\n", my_buffer);
        printf("Buffer Mem align : %d\n", BUF_ALIGN);
        memset(my_buffer, 0, data_size);
    }

    // 4. Config XDMA Device name
    memcpy(my_devname, DEF_RX_DEVICE_NAME, sizeof(DEF_RX_DEVICE_NAME));

    // 14. Open XDMA Device
    if(xdma_api_dev_open(my_devname, 0 /* eop_flush */, &my_rx_xdma_fd)) {
        printf("FAILURE: Could not open %s. Make sure xdma device driver is loaded and you have access rights (maybe use sudo?).\n", my_devname);
        printf("<<< %s\n", __func__);
        return NULL;
    }
    else
    {
        printf("Open XDMA : Success\n\n");
    }

    while(1)
    {
        xdma_write_status = xdma_api_read_to_buffer_with_fd(my_devname, my_rx_xdma_fd, (char *)my_buffer, (data_size), &bytes_rcv);
        if(xdma_write_status == 0)
        {
            // printf("XDMA Write : Success\n");
        }
        else
        {
            printf("XDMA Write : Failed\n");
        }

        for (int i = 0; i < data_size; i++) {
            printf("%02X ", (unsigned char)my_buffer[i]);
        }
        printf("  => Rx Byte : %d", bytes_rcv);
        printf("\n");

        bytes_rcv = 0;
        usleep(20*1000);
    }    

    free(my_buffer);

    return 0;
}


/******************************************************************************
 *                                                                            *
 *         Tx Timestamp Replica FPGA Logic Test Application Function          *
 *                                                                            *
 ******************************************************************************/
int tx_tstamp_replica_fpga_logic_test_app(void)
{
    // 1. Initialize structures
    memset(&my_tx_stats, 0, sizeof(stats_t));
    memset(&tm_now, 0, sizeof(tm_now));
    memset(&tm_prv, 0, sizeof(tm_prv));

    // 2. Register Signal Handler
    register_signal_handler();

    // 3. Open Text file for data logging
    error_log_fd = fopen("Tx_Tstamp_Replica_Error_Log.txt", "w");
    if(error_log_fd == NULL)
    {
        printf("Cannot Open error log file!\n");

        return -1;
    }

    fprintf(error_log_fd, "==================== Tx Tstamp Replica Error Log ====================\n");
    fprintf(error_log_fd, "Packet transmission interval : %ld[ms]\n", time_interval_ms);
    my_time = time(NULL);
    tm_now = *localtime(&my_time);
    fprintf(error_log_fd, "Test start time : %d-%d-%d %d:%d:%d\n", tm_now.tm_year+1900, tm_now.tm_mon+1, tm_now.tm_mday, tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);

    // 4. Determine Ideal difference value & 10% of ideal difference value for given Time interval
    if(time_interval_ms      == INTERVAL_1000MS) ideal_diff = TICK_1000MSEC;        // When sleep time is 1000[ms]
    else if(time_interval_ms == INTERVAL_100MS)  ideal_diff = TICK_100MSEC;         // When sleep time is 100[ms]
    else if(time_interval_ms == INTERVAL_10MS)   ideal_diff = TICK_10MSEC;          // When sleep time is 10[ms]

    ten_percent_of_ideal_diff = ideal_diff * ((double)TOLERANCE_PERCENT / 100.);    // Allowed tolerance is 10% of ideal difference (between current & previous Tx Timestamp value)
    tx_timestamp_diff_allowed_min  = ideal_diff;                                    // Allowed minimum value of Tx Timestamp
    tx_timestamp_diff_allowed_max  = ideal_diff + ten_percent_of_ideal_diff;        // Allowed maximum value of Tx Timestamp


    while(1)
    {
        // 5-1. Show Transmission Index
        printf("Replica's %ldth Tx ", tx_index);

        // 5-2. Set Tx Start bit
        set_register(REG_TX_START_CONFIG, 1);

        // 5-3. Get System Count & Tx Timestamp
        my_syscount     = ((uint64_t)get_register(REG_SYSCLOCK_UPPER) << 32) | get_register(REG_SYSCLOCK_LOWER);
        my_tx_timestamp = ((uint64_t)get_register(REG_TX_TSTAMP_UPPER) << 32) | get_register(REG_TX_TSTAMP_LOWER);

        // 5-4. Increase Statistics (Tx packets, Bytes)
        my_tx_stats.txPackets++;

        // 5-5. Get Current Raspberry PI5 Time
        my_time = time(NULL);
        tm_now  = *localtime(&my_time);

        // 5-6. Calculate diff of Raspberry PI5 Time
        gettimeofday(&tv, NULL);
        rasp_time = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;   // unit : [ms]
        rasp_time_diff = rasp_time - rasp_time_prv;             // unit : [ms]

        // 5-7. Calculate diff of System count & Tx Timestamp
        if(tx_index > 1)
        {
            diff_syscount               = my_syscount - my_syscount_prv;
            diff_tx_timestamp           = my_tx_timestamp - my_tx_timestamp_prv;
            diff_syscount_txtimestamp   = my_syscount - my_tx_timestamp;
        } 

        // 5-8. Configure Error Condition
        if(my_tx_timestamp == my_tx_timestamp_prv)
        {
            // Error Condition 1 : Current & Previous Tx Timestamp is equal
            error_flag = FLAG_SET;
            error_type = 1;
        }
        else if(my_tx_timestamp < my_tx_timestamp_prv)
        {
            // Error Condition 2 : Current Tx Timestamp is less than Previous
            error_flag = FLAG_SET;
            error_type = 2;
        }
        else if((diff_tx_timestamp < tx_timestamp_diff_allowed_min) || (diff_tx_timestamp > tx_timestamp_diff_allowed_max))
        {
            // Error Condition 3 : Tx Timestamp Difference is out of Allowed range
            error_flag = FLAG_SET;
            error_type = 3;
        }
        else if(diff_syscount_txtimestamp > (uint64_t)0xFFFFF)
        {
            // Error Condition 4 : Current Syscount is 0xFFFFF value more than Tx Timestamp
            error_flag = FLAG_SET;
            error_type = 4;
        }
        else
        {
            // No Error
            error_flag = FLAG_CLEAR;
            error_type = 0;
        }

        // 5-9. If Error occurs, Save Error log to Text File
        if(error_flag == FLAG_SET)
        {
            if(tx_index > 1)
            {
                fprintf(error_log_fd, \
                "\n============================= %ldth Error =============================\
                \nRaspberry PI5 OS Time                       : %d-%d-%d %d:%d:%d\
                \nMeasured Transmission Time Gap              : %.1lf[ms]\
                \n\nTransmission Index                          : %ld\
                \nTransmitted packet                          : %lld\
                \n\nsyscount                   (hex)            : %016lx                |  tx_timestamp      (hex) : %016lx\
                \nsyscount_prv               (hex)            : %016lx                |  tx_timestamp_prv  (hex) : %016lx\
                \n\nsyscount_diff              (dec)            : %16ld  (%.4lf[s])   |  tx_timestamp_diff (dec) : %16ld (%.4lf[s])\
                \nsyscount_diff              (hex)            : %16lx  (%.4lf[s])   |  tx_timestamp_diff (hex) : %16lx (%.4lf[s])\
                \n\nsyscount_txtimestamp_diff  (hex)            : %16lx\
                \nsyscount_txtimestamp_diff  (dec)            : %16ld\
                \n", (error_count+1),\
                tm_now.tm_year+1900, tm_now.tm_mon+1, tm_now.tm_mday, tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec,\
                rasp_time_diff,\
                tx_index, my_tx_stats.txPackets,\
                my_syscount, my_tx_timestamp, my_syscount_prv, my_tx_timestamp_prv,\
                diff_syscount, (double)diff_syscount/TICK_1000MSEC, diff_tx_timestamp, (double)diff_tx_timestamp/TICK_1000MSEC,\
                diff_syscount, (double)diff_syscount/TICK_1000MSEC, diff_tx_timestamp, (double)diff_tx_timestamp/TICK_1000MSEC,\
                diff_syscount_txtimestamp, diff_syscount_txtimestamp);

                fprintf(error_log_fd, \
                "\nIdeal Diff                                   : %ld           (=> %.4lf[s])\
                \nAllowed Tx Timestamp Diff Range (%d%% tor.)  : %ld ~ %ld (=> %.4lf[s])\n\n", ideal_diff, (double)ideal_diff/TICK_1000MSEC, TOLERANCE_PERCENT, tx_timestamp_diff_allowed_min, tx_timestamp_diff_allowed_max, (double)tx_timestamp_diff_allowed_max/TICK_1000MSEC);
                
                if(error_type == 1)
                {
                    fprintf(error_log_fd, "Error Cause : Current & Previous Tx Timestamp is equal\n\n");
                }
                else if(error_type == 2)
                {
                    fprintf(error_log_fd, "Error Cause : Current Tx Timestamp is less than Previous\n\n");
                }
                else if(error_type == 3)
                {
                    fprintf(error_log_fd, "Error Cause : Tx Timestamp Difference is out of Allowed range\n\n");
                }
                else if(error_type == 4)
                {
                    fprintf(error_log_fd, "############################################################## Error Cause : Current Syscount is 0xFFFFF value more than Tx Timestamp ##############################################################\n\n");
                }

                error_count++;
            }
        }

        printf("=> total error count : %ld\
        \nsyscount                  (hex) : %016lx               |   tx_timestamp     (hex) : %016lx\
        \nsyscount_prv              (hex) : %016lx               |   tx_timestamp_prv (hex) : %016lx\
        \nsyscount_diff             (dec) : %16ld (%.4lf[s])   |   tx_timestamp_diff(dec) : %16ld (%.4lf[s])\
        \n\nsyscount_txtimestamp_diff (hex) : %16lx                   (Dec : %16ld)\
        \n\n", error_count, my_syscount, my_tx_timestamp, my_syscount_prv, my_tx_timestamp_prv, diff_syscount, (double)diff_syscount/TICK_1000MSEC, diff_tx_timestamp, (double)diff_tx_timestamp/TICK_1000MSEC,\
        diff_syscount_txtimestamp, diff_syscount_txtimestamp);

        // 5-10. Initialize variable for Next transmission
        my_syscount_prv     = my_syscount;
        my_tx_timestamp_prv = my_tx_timestamp;
        rasp_time_prv       = rasp_time;
        memcpy(&tm_prv, &tm_now, sizeof(tm_prv));
        tx_index++;

        // 5-11. Clear Tx Start bit
        set_register(REG_TX_START_CONFIG, 0);

        // 5-12. Wait for Specifed Time Value
        usleep(time_interval_ms*1000);
    }

    // 6. Close Text file for logging data
    fclose(error_log_fd);

    return 0;
}


int process_main_fpga_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl)
{
    return fpga_test_app();
}

int process_main_xdma_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl)
{
    return xdma_rx_test_app();
}

int process_main_tx_tstamp_replica_fpga_logic_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl)
{
    return tx_tstamp_replica_fpga_logic_test_app();
}

/***************************************************************************** */

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

int32_t fn_show_register_genArgument(int32_t argc, const char *argv[]);
int32_t fn_show_register_rxArgument(int32_t argc, const char *argv[]);
int32_t fn_show_register_txArgument(int32_t argc, const char *argv[]);
int32_t fn_show_register_h2cArgument(int32_t argc, const char *argv[]);
int32_t fn_show_register_c2hArgument(int32_t argc, const char *argv[]);
int32_t fn_show_register_irqArgument(int32_t argc, const char *argv[]);
int32_t fn_show_register_configArgument(int32_t argc, const char *argv[]);
int32_t fn_show_register_h2c_sgdmaArgument(int32_t argc, const char *argv[]);
int32_t fn_show_register_c2h_sgdmaArgument(int32_t argc, const char *argv[]);
int32_t fn_show_register_common_sgdmaArgument(int32_t argc, const char *argv[]);
int32_t fn_show_register_msix_vectorArgument(int32_t argc, const char *argv[]);

argument_list_t  showRegisterArgument_tbl[] = {
        {"gen",  fn_show_register_genArgument},
        {"rx",   fn_show_register_rxArgument},
        {"tx",   fn_show_register_txArgument},
        {"h2c",  fn_show_register_h2cArgument},
        {"c2h",  fn_show_register_c2hArgument},
        {"irq",  fn_show_register_irqArgument},
        {"con",  fn_show_register_configArgument},
        {"h2cs", fn_show_register_h2c_sgdmaArgument},
        {"c2hs", fn_show_register_c2h_sgdmaArgument},
        {"com",  fn_show_register_common_sgdmaArgument},
        {"msix", fn_show_register_msix_vectorArgument},
        {0,     NULL}
    };

int fn_show_registerArgument(int argc, const char *argv[]);
argument_list_t  showArgument_tbl[] = {
        {"register", fn_show_registerArgument},
        {0,          NULL}
    };

int32_t fn_set_register_genArgument(int32_t argc, const char *argv[]);
int32_t fn_set_register_h2cArgument(int32_t argc, const char *argv[]);
int32_t fn_set_register_c2hArgument(int32_t argc, const char *argv[]);
int32_t fn_set_register_irqArgument(int32_t argc, const char *argv[]);
int32_t fn_set_register_configArgument(int32_t argc, const char *argv[]);
int32_t fn_set_register_h2c_sgdmaArgument(int32_t argc, const char *argv[]);
int32_t fn_set_register_c2h_sgdmaArgument(int32_t argc, const char *argv[]);
int32_t fn_set_register_common_sgdmaArgument(int32_t argc, const char *argv[]);
int32_t fn_set_register_msix_vectorArgument(int32_t argc, const char *argv[]);

argument_list_t  setRegisterArgument_tbl[] = {
        {"gen",  fn_set_register_genArgument},
        {"rx",   fn_set_register_genArgument},
        {"tx",   fn_set_register_genArgument},
        {"h2c",  fn_set_register_h2cArgument},
        {"c2h",  fn_set_register_c2hArgument},
        {"irq",  fn_set_register_irqArgument},
        {"con",  fn_set_register_configArgument},
        {"h2cs", fn_set_register_h2c_sgdmaArgument},
        {"c2hs", fn_set_register_c2h_sgdmaArgument},
        {"com",  fn_set_register_common_sgdmaArgument},
        {"msix", fn_set_register_msix_vectorArgument},
        {0,     NULL}
    };

int fn_set_registerArgument(int argc, const char *argv[]);
argument_list_t  setArgument_tbl[] = {
        {"register", fn_set_registerArgument},
        {0,          NULL}
    };

#define XDMA_REGISTER_DEV    "/dev/xdma0_user"

int set_register(int offset, uint32_t val) {

    return xdma_api_wr_register(XDMA_REGISTER_DEV, offset, 'w', val);
}

uint32_t get_register(int offset) {

    uint32_t read_val = 0;
    xdma_api_rd_register(XDMA_REGISTER_DEV, offset, 'w', &read_val);

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


#define XDMA_ENGINE_REGISTER_DEV    "/dev/xdma0_control"
uint32_t get_xdma_engine_register(int offset) {

    uint32_t read_val = 0;
    xdma_api_rd_register(XDMA_ENGINE_REGISTER_DEV, offset, 'w', &read_val);

    return read_val;
}

void xdma_reginfo(struct reginfo* reginfo) {

    for (int i = 0; reginfo[i].offset >= 0; i++) {
        if (reginfo[i].name[0] == '@') {
            uint64_t ll = get_xdma_engine_register(reginfo[i].offset);
            ll = (ll << 32) | get_xdma_engine_register(reginfo[i].offset + 4);
            printf("%s : 0x%lx\n", &(reginfo[i].name[1]), ll);
        } else {
            printf("%s : 0x%x\n", reginfo[i].name, (unsigned int)get_xdma_engine_register(reginfo[i].offset));
        }
    }
}

#define H2C_CHANNEL_IDENTIFIER_0x00                   (0X00)
#define H2C_CHANNEL_CONTROL_0X04                      (0X04)
#define H2C_CHANNEL_CONTROL_0X08                      (0X08)
#define H2C_CHANNEL_CONTROL_0X0C                      (0X0C)
#define H2C_CHANNEL_STATUS_0X40                       (0X40)
#define H2C_CHANNEL_STATUS_0X44                       (0X44)
#define H2C_CHANNEL_COMPLETED_DESCRIPTOR_COUNT_0X48   (0X48)
#define H2C_CHANNEL_ALIGNMENTS_0X4C                   (0X4C)
#define H2C_POLL_MODE_LOW_WRITE_BACK_ADDRESS_0X88     (0X88)
#define H2C_POLL_MODE_HIGH_WRITE_BACK_ADDRESS_0X8C    (0X8C)
#define H2C_CHANNEL_INTERRUPT_ENABLE_MASK_0X90        (0X90)
#define H2C_CHANNEL_INTERRUPT_ENABLE_MASK_0X94        (0X94)
#define H2C_CHANNEL_INTERRUPT_ENABLE_MASK_0X98        (0X98)
#define H2C_CHANNEL_PERFORMANCE_MONITOR_CONTROL_0XC0  (0XC0)
#define H2C_CHANNEL_PERFORMANCE_CYCLE_COUNT_0XC4      (0XC4)
#define H2C_CHANNEL_PERFORMANCE_CYCLE_COUNT_0XC8      (0XC8)
#define H2C_CHANNEL_PERFORMANCE_DATA_COUNT_0XCC       (0XCC)
#define H2C_CHANNEL_PERFORMANCE_DATA_COUNT_0XD0       (0XD0)

struct reginfo reg_h2c[] = {
    {"H2C Channel Identifier (0x00)",                  H2C_CHANNEL_IDENTIFIER_0x00},
    {"H2C Channel Control (0x04)",                     H2C_CHANNEL_CONTROL_0X04},
    {"H2C Channel Control (0x08)",                     H2C_CHANNEL_CONTROL_0X08},
    {"H2C Channel Control (0x0C)",                     H2C_CHANNEL_CONTROL_0X0C},
    {"H2C Channel Status (0x40)",                      H2C_CHANNEL_STATUS_0X40},
    {"H2C Channel Status (0x44)",                      H2C_CHANNEL_STATUS_0X44},
    {"H2C Channel Completed Descriptor Count (0x48)",  H2C_CHANNEL_COMPLETED_DESCRIPTOR_COUNT_0X48},
    {"H2C Channel Alignments (0x4C)",                  H2C_CHANNEL_ALIGNMENTS_0X4C},
    {"H2C Poll Mode Low Write Back Address (0x88)",    H2C_POLL_MODE_LOW_WRITE_BACK_ADDRESS_0X88},
    {"H2C Poll Mode High Write Back Address (0x8C)",   H2C_POLL_MODE_HIGH_WRITE_BACK_ADDRESS_0X8C},
    {"H2C Channel Interrupt Enable Mask (0x90)",       H2C_CHANNEL_INTERRUPT_ENABLE_MASK_0X90},
    {"H2C Channel Interrupt Enable Mask (0x94)",       H2C_CHANNEL_INTERRUPT_ENABLE_MASK_0X94},
    {"H2C Channel Interrupt Enable Mask (0x98)",       H2C_CHANNEL_INTERRUPT_ENABLE_MASK_0X98},
    {"H2C Channel Performance Monitor Control (0xC0)", H2C_CHANNEL_PERFORMANCE_MONITOR_CONTROL_0XC0},
    {"H2C Channel Performance Cycle Count (0xC4)",     H2C_CHANNEL_PERFORMANCE_CYCLE_COUNT_0XC4},
    {"H2C Channel Performance Cycle Count (0xC8)",     H2C_CHANNEL_PERFORMANCE_CYCLE_COUNT_0XC8},
    {"H2C Channel Performance Data Count (0xCC)",      H2C_CHANNEL_PERFORMANCE_DATA_COUNT_0XCC},
    {"H2C Channel Performance Data Count (0xD0)",      H2C_CHANNEL_PERFORMANCE_DATA_COUNT_0XD0},
    {"", -1}
};

#define    C2H_CHANNEL_IDENTIFIER_0X00                  (0x1000 + 0x00)
#define    C2H_CHANNEL_CONTROL_0X04                     (0x1000 + 0x04)
#define    C2H_CHANNEL_CONTROL_0X08                     (0x1000 + 0x08)
#define    C2H_CHANNEL_CONTROL_0X0C                     (0x1000 + 0x0C)
#define    C2H_CHANNEL_STATUS_0X40                      (0x1000 + 0x40)
#define    C2H_CHANNEL_STATUS_0X44                      (0x1000 + 0x44)
#define    C2H_CHANNEL_COMPLETED_DESCRIPTOR_COUNT_0X48  (0x1000 + 0x48)
#define    C2H_CHANNEL_ALIGNMENTS_0X4C                  (0x1000 + 0x4C)
#define    C2H_POLL_MODE_LOW_WRITE_BACK_ADDRESS_0X88    (0x1000 + 0x88)
#define    C2H_POLL_MODE_HIGH_WRITE_BACK_ADDRESS_0X8C   (0x1000 + 0x8C)
#define    C2H_CHANNEL_INTERRUPT_ENABLE_MASK_0X90       (0x1000 + 0x90)
#define    C2H_CHANNEL_INTERRUPT_ENABLE_MASK_0X94       (0x1000 + 0x94)
#define    C2H_CHANNEL_INTERRUPT_ENABLE_MASK_0X98       (0x1000 + 0x98)
#define    C2H_CHANNEL_PERFORMANCE_MONITOR_CONTROL_0XC0 (0x1000 + 0xC0)
#define    C2H_CHANNEL_PERFORMANCE_CYCLE_COUNT_0XC4     (0x1000 + 0xC4)
#define    C2H_CHANNEL_PERFORMANCE_CYCLE_COUNT_0XC8     (0x1000 + 0xC8)
#define    C2H_CHANNEL_PERFORMANCE_DATA_COUNT_0XCC      (0x1000 + 0xCC)
#define    C2H_CHANNEL_PERFORMANCE_DATA_COUNT_0XD0      (0x1000 + 0xD0)

struct reginfo reg_c2h[] = {
    {"C2H Channel Identifier (0x00)",                  C2H_CHANNEL_IDENTIFIER_0X00},
    {"C2H Channel Control (0x04)",                     C2H_CHANNEL_CONTROL_0X04},
    {"C2H Channel Control (0x08)",                     C2H_CHANNEL_CONTROL_0X08},
    {"C2H Channel Control (0x0C)",                     C2H_CHANNEL_CONTROL_0X0C},
    {"C2H Channel Status (0x40)",                      C2H_CHANNEL_STATUS_0X40},
    {"C2H Channel Status (0x44)",                      C2H_CHANNEL_STATUS_0X44},
    {"C2H Channel Completed Descriptor Count (0x48)",  C2H_CHANNEL_COMPLETED_DESCRIPTOR_COUNT_0X48},
    {"C2H Channel Alignments (0x4C)",                  C2H_CHANNEL_ALIGNMENTS_0X4C},
    {"C2H Poll Mode Low Write Back Address (0x88)",    C2H_POLL_MODE_LOW_WRITE_BACK_ADDRESS_0X88},
    {"C2H Poll Mode High Write Back Address (0x8C)",   C2H_POLL_MODE_HIGH_WRITE_BACK_ADDRESS_0X8C},
    {"C2H Channel Interrupt Enable Mask (0x90)",       C2H_CHANNEL_INTERRUPT_ENABLE_MASK_0X90},
    {"C2H Channel Interrupt Enable Mask (0x94)",       C2H_CHANNEL_INTERRUPT_ENABLE_MASK_0X94},
    {"C2H Channel Interrupt Enable Mask (0x98)",       C2H_CHANNEL_INTERRUPT_ENABLE_MASK_0X98},
    {"C2H Channel Performance Monitor Control (0xC0)", C2H_CHANNEL_PERFORMANCE_MONITOR_CONTROL_0XC0},
    {"C2H Channel Performance Cycle Count (0xC4)",     C2H_CHANNEL_PERFORMANCE_CYCLE_COUNT_0XC4},
    {"C2H Channel Performance Cycle Count (0xC8)",     C2H_CHANNEL_PERFORMANCE_CYCLE_COUNT_0XC8},
    {"C2H Channel Performance Data Count (0xCC)",      C2H_CHANNEL_PERFORMANCE_DATA_COUNT_0XCC},
    {"C2H Channel Performance Data Count (0xD0)",      C2H_CHANNEL_PERFORMANCE_DATA_COUNT_0XD0},
    {"", -1}
};

#define IRQ_BLOCK_IDENTIFIER_0X00                    (0x2000 + 0x00)
#define IRQ_BLOCK_USER_INTERRUPT_ENABLE_MASK_0X04    (0x2000 + 0x04)
#define IRQ_BLOCK_USER_INTERRUPT_ENABLE_MASK_0X08    (0x2000 + 0x08)
#define IRQ_BLOCK_USER_INTERRUPT_ENABLE_MASK_0X0C    (0x2000 + 0x0C)
#define IRQ_BLOCK_CHANNEL_INTERRUPT_ENABLE_MASK_0X10 (0x2000 + 0x10)
#define IRQ_BLOCK_CHANNEL_INTERRUPT_ENABLE_MASK_0X14 (0x2000 + 0x14)
#define IRQ_BLOCK_CHANNEL_INTERRUPT_ENABLE_MASK_0X18 (0x2000 + 0x18)
#define IRQ_BLOCK_USER_INTERRUPT_REQUEST_0X40        (0x2000 + 0x40)
#define IRQ_BLOCK_CHANNEL_INTERRUPT_REQUEST_0X44     (0x2000 + 0x44)
#define IRQ_BLOCK_USER_INTERRUPT_PENDING_0X48        (0x2000 + 0x48)
#define IRQ_BLOCK_CHANNEL_INTERRUPT_PENDING_0X4C     (0x2000 + 0x4C)
#define IRQ_BLOCK_USER_VECTOR_NUMBER_0X80            (0x2000 + 0x80)
#define IRQ_BLOCK_USER_VECTOR_NUMBER_0X84            (0x2000 + 0x84)
#define IRQ_BLOCK_USER_VECTOR_NUMBER_0X88            (0x2000 + 0x88)
#define IRQ_BLOCK_USER_VECTOR_NUMBER_0X8C            (0x2000 + 0x8C)
#define IRQ_BLOCK_CHANNEL_VECTOR_NUMBER_0XA0         (0x2000 + 0xA0)
#define IRQ_BLOCK_CHANNEL_VECTOR_NUMBER_0XA4         (0x2000 + 0xA4)

struct reginfo reg_irq[] = {
    {"IRQ Block Identifier (0x00)",                    IRQ_BLOCK_IDENTIFIER_0X00},
    {"IRQ Block User Interrupt Enable Mask (0x04)",    IRQ_BLOCK_USER_INTERRUPT_ENABLE_MASK_0X04},
    {"IRQ Block User Interrupt Enable Mask (0x08)",    IRQ_BLOCK_USER_INTERRUPT_ENABLE_MASK_0X08},
    {"IRQ Block User Interrupt Enable Mask (0x0C)",    IRQ_BLOCK_USER_INTERRUPT_ENABLE_MASK_0X0C},
    {"IRQ Block Channel Interrupt Enable Mask (0x10)", IRQ_BLOCK_CHANNEL_INTERRUPT_ENABLE_MASK_0X10},
    {"IRQ Block Channel Interrupt Enable Mask (0x14)", IRQ_BLOCK_CHANNEL_INTERRUPT_ENABLE_MASK_0X14},
    {"IRQ Block Channel Interrupt Enable Mask (0x18)", IRQ_BLOCK_CHANNEL_INTERRUPT_ENABLE_MASK_0X18},
    {"IRQ Block User Interrupt Request (0x40)",        IRQ_BLOCK_USER_INTERRUPT_REQUEST_0X40},
    {"IRQ Block Channel Interrupt Request (0x44)",     IRQ_BLOCK_CHANNEL_INTERRUPT_REQUEST_0X44},
    {"IRQ Block User Interrupt Pending (0x48)",        IRQ_BLOCK_USER_INTERRUPT_PENDING_0X48},
    {"IRQ Block Channel Interrupt Pending (0x4C)",     IRQ_BLOCK_CHANNEL_INTERRUPT_PENDING_0X4C},
    {"IRQ Block User Vector Number (0x80)",            IRQ_BLOCK_USER_VECTOR_NUMBER_0X80},
    {"IRQ Block User Vector Number (0x84)",            IRQ_BLOCK_USER_VECTOR_NUMBER_0X84},
    {"IRQ Block User Vector Number (0x88)",            IRQ_BLOCK_USER_VECTOR_NUMBER_0X88},
    {"IRQ Block User Vector Number (0x8C)",            IRQ_BLOCK_USER_VECTOR_NUMBER_0X8C},
    {"IRQ Block Channel Vector Number (0xA0)",         IRQ_BLOCK_CHANNEL_VECTOR_NUMBER_0XA0},
    {"IRQ Block Channel Vector Number (0xA4)",         IRQ_BLOCK_CHANNEL_VECTOR_NUMBER_0XA4},
    {"", -1}
};


#define CONFIG_BLOCK_IDENTIFIER_0X00                 (0x3000 + 0x00)
#define CONFIG_BLOCK_BUSDEV_0X04                     (0x3000 + 0x04)
#define CONFIG_BLOCK_PCIE_MAX_PAYLOAD_SIZE_0X08      (0x3000 + 0x08)
#define CONFIG_BLOCK_PCIE_MAX_READ_REQUEST_SIZE_0X0C (0x3000 + 0x0C)
#define CONFIG_BLOCK_SYSTEM_ID_0X10                  (0x3000 + 0x10)
#define CONFIG_BLOCK_MSI_ENABLE_0X14                 (0x3000 + 0x14)
#define CONFIG_BLOCK_PCIE_DATA_WIDTH_0X18            (0x3000 + 0x18)
#define CONFIG_PCIE_CONTROL_0X1C                     (0x3000 + 0x1C)
#define CONFIG_AXI_USER_MAX_PAYLOAD_SIZE_0X40        (0x3000 + 0x40)
#define CONFIG_AXI_USER_MAX_READ_REQUEST_SIZE_0X44   (0x3000 + 0x44)
#define CONFIG_WRITE_FLUSH_TIMEOUT_0X60              (0x3000 + 0x60)

struct reginfo reg_config[] = {
    {"Config Block Identifier (0x00)",                 CONFIG_BLOCK_IDENTIFIER_0X00},
    {"Config Block BusDev (0x04)",                     CONFIG_BLOCK_BUSDEV_0X04},
    {"Config Block PCIE Max Payload Size (0x08)",      CONFIG_BLOCK_PCIE_MAX_PAYLOAD_SIZE_0X08},
    {"Config Block PCIE Max Read Request Size (0x0C)", CONFIG_BLOCK_PCIE_MAX_READ_REQUEST_SIZE_0X0C},
    {"Config Block System ID (0x10)",                  CONFIG_BLOCK_SYSTEM_ID_0X10},
    {"Config Block MSI Enable (0x14)",                 CONFIG_BLOCK_MSI_ENABLE_0X14},
    {"Config Block PCIE Data Width (0x18)",            CONFIG_BLOCK_PCIE_DATA_WIDTH_0X18},
    {"Config PCIE Control (0x1C)",                     CONFIG_PCIE_CONTROL_0X1C},
    {"Config AXI User Max Payload Size (0x40)",        CONFIG_AXI_USER_MAX_PAYLOAD_SIZE_0X40},
    {"Config AXI User Max Read Request Size (0x44)",   CONFIG_AXI_USER_MAX_READ_REQUEST_SIZE_0X44},
    {"Config Write Flush Timeout (0x60)",              CONFIG_WRITE_FLUSH_TIMEOUT_0X60},
    {"", -1}
};

#define H2C_SGDMA_IDENTIFIER_0X00              (0x4000 + 0x00)
#define H2C_SGDMA_DESCRIPTOR_LOW_ADDRESS_0X80  (0x4000 + 0x80)
#define H2C_SGDMA_DESCRIPTOR_HIGH_ADDRESS_0X84 (0x4000 + 0x84)
#define H2C_SGDMA_DESCRIPTOR_ADJACENT_0X88     (0x4000 + 0x88)
#define H2C_SGDMA_DESCRIPTOR_CREDITS_0X8C      (0x4000 + 0x8C)

struct reginfo reg_h2c_sgdma[] = {
    {"H2C SGDMA Identifier (0x00)",              H2C_SGDMA_IDENTIFIER_0X00},
    {"H2C SGDMA Descriptor Low Address (0x80)",  H2C_SGDMA_DESCRIPTOR_LOW_ADDRESS_0X80},
    {"H2C SGDMA Descriptor High Address (0x84)", H2C_SGDMA_DESCRIPTOR_HIGH_ADDRESS_0X84},
    {"H2C SGDMA Descriptor Adjacent (0x88)",     H2C_SGDMA_DESCRIPTOR_ADJACENT_0X88},
    {"H2C SGDMA Descriptor Credits (0x8C)",      H2C_SGDMA_DESCRIPTOR_CREDITS_0X8C},
    {"", -1}
};

#define C2H_SGDMA_IDENTIFIER_0X00              (0x5000 + 0x00)
#define C2H_SGDMA_DESCRIPTOR_LOW_ADDRESS_0X80  (0x5000 + 0x80)
#define C2H_SGDMA_DESCRIPTOR_HIGH_ADDRESS_0X84 (0x5000 + 0x84)
#define C2H_SGDMA_DESCRIPTOR_ADJACENT_0X88     (0x5000 + 0x88)
#define C2H_SGDMA_DESCRIPTOR_CREDITS_0X8C      (0x5000 + 0x8C)
  
struct reginfo reg_c2h_sgdma[] = {
    {"C2H SGDMA Identifier (0x00)",              C2H_SGDMA_IDENTIFIER_0X00},
    {"C2H SGDMA Descriptor Low Address (0x80)",  C2H_SGDMA_DESCRIPTOR_LOW_ADDRESS_0X80},
    {"C2H SGDMA Descriptor High Address (0x84)", C2H_SGDMA_DESCRIPTOR_HIGH_ADDRESS_0X84},
    {"C2H SGDMA Descriptor Adjacent (0x88)",     C2H_SGDMA_DESCRIPTOR_ADJACENT_0X88},
    {"C2H SGDMA Descriptor Credits (0x8C)",      C2H_SGDMA_DESCRIPTOR_CREDITS_0X8C},
    {"", -1}
};


#define SGDMA_IDENTIFIER_REGISTERS_0X00          (0x6000 + 0x00)
#define SGDMA_DESCRIPTOR_CONTROL_REGISTER_0X10   (0x6000 + 0x10)
#define SGDMA_DESCRIPTOR_CONTROL_REGISTER_0X14   (0x6000 + 0x14)
#define SGDMA_DESCRIPTOR_CONTROL_REGISTER_0X18   (0x6000 + 0x18)
#define SGDMA_DESCRIPTOR_CREDIT_MODE_ENABLE_0X20 (0x6000 + 0x20)
#define SG_DESCRIPTOR_MODE_ENABLE_REGISTER_0X24  (0x6000 + 0x24)
#define SG_DESCRIPTOR_MODE_ENABLE_REGISTER_0X28  (0x6000 + 0x28)

struct reginfo reg_common_sgdma[] = {
    {"SGDMA Identifier Registers (0x00)",          SGDMA_IDENTIFIER_REGISTERS_0X00},
    {"SGDMA Descriptor Control Register (0x10)",   SGDMA_DESCRIPTOR_CONTROL_REGISTER_0X10},
    {"SGDMA Descriptor Control Register (0x14)",   SGDMA_DESCRIPTOR_CONTROL_REGISTER_0X14},
    {"SGDMA Descriptor Control Register (0x18)",   SGDMA_DESCRIPTOR_CONTROL_REGISTER_0X18},
    {"SGDMA Descriptor Credit Mode Enable (0x20)", SGDMA_DESCRIPTOR_CREDIT_MODE_ENABLE_0X20},
    {"SG Descriptor Mode Enable Register (0x24)",  SG_DESCRIPTOR_MODE_ENABLE_REGISTER_0X24},
    {"SG Descriptor Mode Enable Register (0x28)",  SG_DESCRIPTOR_MODE_ENABLE_REGISTER_0X28},
    {"", -1}
};

#define MSI_X_VECTOR0_MESSAGE_LOWER_ADDRESS  (0x8000 + 0x00)
#define MSI_X_VECTOR0_MESSAGE_UPPER_ADDRESS  (0x8000 + 0x04)
#define MSI_X_VECTOR0_MESSAGE_DATA           (0x8000 + 0x08)
#define MSI_X_VECTOR0_CONTROL                (0x8000 + 0x0C)
#define MSI_X_VECTOR31_MESSAGE_LOWER_ADDRESS (0x8000 + 0x1F0)
#define MSI_X_VECTOR31_MESSAGE_UPPER_ADDRESS (0x8000 + 0x1F4)
#define MSI_X_VECTOR31_MESSAGE_DATA          (0x8000 + 0x1F8)
#define MSI_X_VECTOR31_CONTROL               (0x8000 + 0x1FC)
#define MSI_X_PENDING_BIT_ARRAY              (0x8000 + 0xFE0)

struct reginfo reg_msix_vector[] = {
    {"MSI-X vector0 message lower address.",  MSI_X_VECTOR0_MESSAGE_LOWER_ADDRESS},
    {"MSI-X vector0 message upper address.",  MSI_X_VECTOR0_MESSAGE_UPPER_ADDRESS},
    {"MSI-X vector0 message data.",           MSI_X_VECTOR0_MESSAGE_DATA},
    {"MSI-X vector0 control.",                MSI_X_VECTOR0_CONTROL},
    {"MSI-X vector31 message lower address.", MSI_X_VECTOR31_MESSAGE_LOWER_ADDRESS},
    {"MSI-X vector31 message upper address.", MSI_X_VECTOR31_MESSAGE_UPPER_ADDRESS},
    {"MSI-X vector31 message data.",          MSI_X_VECTOR31_MESSAGE_DATA},
    {"MSI-X vector31 control.",               MSI_X_VECTOR31_CONTROL},
    {"MSI-X Pending Bit Array",               MSI_X_PENDING_BIT_ARRAY},
    {"", -1}
};


void dump_registers(int dumpflag, int on) {
    printf("==== Register Dump[%d] Start ====\n", on);
    if (dumpflag & DUMPREG_GENERAL) dump_reginfo(reg_general);
    if (dumpflag & DUMPREG_RX) dump_reginfo(reg_rx);
    if (dumpflag & DUMPREG_TX) dump_reginfo(reg_tx);
    if (dumpflag & XDMA_REG_H2C) xdma_reginfo(reg_h2c);
    if (dumpflag & XDMA_REG_C2H) xdma_reginfo(reg_c2h);
    if (dumpflag & XDMA_REG_IRQ) xdma_reginfo(reg_irq);
    if (dumpflag & XDMA_REG_CON) xdma_reginfo(reg_config);
    if (dumpflag & XDMA_REG_H2CS) xdma_reginfo(reg_h2c_sgdma);
    if (dumpflag & XDMA_REG_C2HS) xdma_reginfo(reg_c2h_sgdma);
    if (dumpflag & XDMA_REG_SCOM) xdma_reginfo(reg_common_sgdma);
    if (dumpflag & XDMA_REG_MSIX) xdma_reginfo(reg_msix_vector);
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

sysclock_t get_my_count() {

    // return get_register(REG_UP_COUNTER_LOW);
    return ((uint64_t)get_register(REG_DN_COUNTER_HIGH) << 32) | get_register(REG_DN_COUNTER_LOW);
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

int32_t fn_show_register_h2cArgument(int32_t argc, const char *argv[]) {

    dump_registers(XDMA_REG_H2C, 1);
    return 0;
}

int32_t fn_show_register_c2hArgument(int32_t argc, const char *argv[]) {

    dump_registers(XDMA_REG_C2H, 1);
    return 0;
}

int32_t fn_show_register_irqArgument(int32_t argc, const char *argv[]) {

    dump_registers(XDMA_REG_IRQ, 1);
    return 0;
}

int32_t fn_show_register_configArgument(int32_t argc, const char *argv[]) {

    dump_registers(XDMA_REG_CON, 1);
    return 0;
}

int32_t fn_show_register_h2c_sgdmaArgument(int32_t argc, const char *argv[]) {

    dump_registers(XDMA_REG_H2CS, 1);
    return 0;
}

int32_t fn_show_register_c2h_sgdmaArgument(int32_t argc, const char *argv[]) {

    dump_registers(XDMA_REG_C2HS, 1);
    return 0;
}

int32_t fn_show_register_common_sgdmaArgument(int32_t argc, const char *argv[]) {

    dump_registers(XDMA_REG_SCOM, 1);
    return 0;
}

int32_t fn_show_register_msix_vectorArgument(int32_t argc, const char *argv[]) {

    dump_registers(XDMA_REG_MSIX, 1);
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

int fn_set_register_genArgument(int argc, const char *argv[]) {

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

#define XDMA_CONTROL_REGISTER_DEV    "/dev/xdma0_control"

int set_xdma_register(int offset, uint32_t val) {

    return xdma_api_wr_register(XDMA_CONTROL_REGISTER_DEV, offset, 'w', val);
}

uint32_t get_xdma_register(int offset) {

    uint32_t read_val = 0;
    xdma_api_rd_register(XDMA_CONTROL_REGISTER_DEV, offset, 'w', &read_val);

    return read_val;
}

int fn_set_xdma_registerArgument(int argc, const char *argv[], int offset) {

    int32_t addr = 0x0; // Scratch Register
    int32_t value = 0x0;

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
        } else {
            printf("value are missed.\r\n");
            return ERR_PARAMETER_MISSED;
        }
    } else {
        printf("address & value are missed.\r\n");
        return ERR_PARAMETER_MISSED;
    }

    if(addr % 4) {
        printf("The address value(0x%08x) does not align with 4-byte alignment.\r\n", addr);
        return ERR_INVALID_PARAMETER;
    }

    set_xdma_register(addr + offset, value);
    printf("address(%08x): %08x\n", addr + offset, get_xdma_register(addr + offset));

    return 0;
}

int fn_set_register_msix_vectorArgument(int argc, const char *argv[]) {
    return fn_set_xdma_registerArgument(argc, argv, 0x8000);
}

int fn_set_register_common_sgdmaArgument(int argc, const char *argv[]) {
    return fn_set_xdma_registerArgument(argc, argv, 0x6000);
}

int fn_set_register_c2h_sgdmaArgument(int argc, const char *argv[]) {
    return fn_set_xdma_registerArgument(argc, argv, 0x5000);
}

int fn_set_register_h2c_sgdmaArgument(int argc, const char *argv[]) {
    return fn_set_xdma_registerArgument(argc, argv, 0x4000);
}

int fn_set_register_configArgument(int argc, const char *argv[]) {
    return fn_set_xdma_registerArgument(argc, argv, 0x3000);
}

int fn_set_register_irqArgument(int argc, const char *argv[]) {
    return fn_set_xdma_registerArgument(argc, argv, 0x2000);
}

int fn_set_register_c2hArgument(int argc, const char *argv[]) {
    return fn_set_xdma_registerArgument(argc, argv, 0x1000);
}

int fn_set_register_h2cArgument(int argc, const char *argv[]) {
    return fn_set_xdma_registerArgument(argc, argv, 0x0000);
}

int fn_set_registerArgument(int argc, const char *argv[]) {

    if(argc <= 0) {
        printf("%s needs a parameter\r\n", __func__);
        return ERR_PARAMETER_MISSED;
    }

    for (int index = 0; setRegisterArgument_tbl[index].name; index++) {
        if (!strcmp(argv[0], setRegisterArgument_tbl[index].name)) {
            argv++, argc--;
            setRegisterArgument_tbl[index].fP(argc, argv);
            return 0;
        }
    }

    return ERR_INVALID_PARAMETER;
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

#ifdef ONE_QUEUE_TSN

int test_fpga_register_rd_wr(uint32_t count);
int send_1queueTSN_packet(char* ip_address, uint32_t from_tick, uint32_t margin);

int test_fpga_register_rd_wr(uint32_t count) {

    int32_t addr = 0x0010; // Scratch Register
    int32_t value;
    int32_t version = 0x24012601;

    for(uint32_t loop = 1; loop <= count; loop++) {
        set_register(addr, loop);
        value = get_register(0);
        if(version != value) {
            printf("version value changed from 0x%08x to 0x%08x\n", version, value);
            version = value;
        }
        value = get_register(addr);
        if(loop != value) {
            printf("\n  [Fail] scratch register write(%d) and read(%d)\n", loop, value);
            return -1;
        }
        if((loop % 1000) == 0) {
            printf("  %10dth loop in progress\r", loop);
            fflush(stdout);
        }
    }

    printf("\n  [Success] Write/read scratch register %d times\n", value);

    return 0;
}

int process_main_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
#define MAIN_TEST_OPTION_STRING  "c:hv"
int process_main_testCmd(int argc, const char *argv[],
                            menu_command_t *menu_tbl) {
    uint32_t count = 100;
    int argflag;

    while ((argflag = getopt(argc, (char **)argv,
                             MAIN_TEST_OPTION_STRING)) != -1) {
        switch (argflag) {
        case 'c':
            if (str2uint(optarg, &count) != 0) {
                printf("Invalid parameter given or out of range for '-c'.");
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

    return test_fpga_register_rd_wr(count);
}

#define MAIN_SEND_OPTION_STRING  "i:f:m:hv"
int process_main_sendCmd(int argc, const char *argv[],
                            menu_command_t *menu_tbl) {
    char ip_address[INET_ADDRSTRLEN] = "192.168.100.10";
    uint32_t from_tick = 12500;
    uint32_t margin = 5000;
    int argflag;

    while ((argflag = getopt(argc, (char **)argv,
                             MAIN_SEND_OPTION_STRING)) != -1) {
        switch (argflag) {
        case 'i':
            memset(ip_address, 0, INET_ADDRSTRLEN);
            strcpy(ip_address, optarg);
            break;
        case 'f':
            if (str2uint(optarg, &from_tick) != 0) {
                printf("Invalid parameter given or out of range for '-f'.");
                return -1;
            }
            break;
        case 'm':
            if (str2uint(optarg, &margin) != 0) {
                printf("Invalid parameter given or out of range for '-t'.");
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

    return send_1queueTSN_packet(ip_address, from_tick, margin);
}
#endif

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
    int t_argc;
    char **pav = NULL;

    for(id=0; id<argc; id++) {
        debug_printf("argv[%d] : %s\n", id, argv[id]);
    }
    debug_printf("\n");

    t_argc = argc, pav = argv;
    pav++, t_argc--;

    return command_parser(t_argc, pav);
}

