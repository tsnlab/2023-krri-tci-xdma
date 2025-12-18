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

/* ================================================================================================================================================ */
//                                                                                                                                                  //
//                                                                                                                                                  //
//                                                      Application SW Codes by Ganghyeok                                                           //
//                                                                                                                                                  //
//                                                                                                                                                  //
/* ================================================================================================================================================ */

// ======================================================================================== //
//                                                                                          //
//  [1] Function Prototypes                                                                 //
//                                                                                          //
// ======================================================================================== //
    // 1. Function Prototypes which are Commonly Used
    void common_register_signal_handler(void);
    void common_signal_stop_handler(void);
    void common_xdma_signal_handler(int sig);

    // 2. Function Prototypes of Frame Transmission Test
    int process_main_frame_transmit_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
    int frame_tx_test_app(int tx_payload_size);
    void app1_init(int tx_payload_size);
    void app1_buffer_alloc(int tx_payload_size);
    void app1_tx_frame_construct(void);
    void app1_exit_task(void);

    // 2-1. Various Type of Frames
    void frame_type_datapath_vrfy_1(uint16_t frame_length);
    void frame_type_datapath_vrfy_2(uint16_t frame_length);
    void frame_type_ieee802(uint16_t frame_length);
    void frame_type_vlan(uint16_t frame_length);
    void frame_type_dix2(uint16_t frame_length);
    void frame_type_jumbo(uint16_t frame_length);

    // 3. Function Prototypes of Frame Reception Test
    int process_main_frame_receive_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
    int frame_rx_test_app(void);
    void app2_init(void);
    void app2_buffer_alloc(int data_size);
    void app2_xdma_read(void);
    void app2_rx_meta_read(void);
    void app2_rx_frame_read(void);

    // 4. Function Prototypes of Register Read/Write Test
    int process_main_register_read_write_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
    int register_rw_test_app(void);

    // 5. Ethernet Path Selection
    int process_main_sel_tx_path_1_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
    int process_main_sel_tx_path_2_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
    int sel_tx_path_1_test_app(void);
    int sel_tx_path_2_test_app(void);

    int process_main_sel_rx_path_1_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
    int process_main_sel_rx_path_2_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl);
    int sel_rx_path_1_test_app(void);
    int sel_rx_path_2_test_app(void);


// ======================================================================================== //
//                                                                                          //
//  [2] Common Macro & Variables                                                            //
//                                                                                          //
// ======================================================================================== //

// ================================================================ //
//   # Macro definition                                             //
// ================================================================ //


// ================================================================ //
//   # Variable definition                                          //
// ================================================================ //

// ======================================================================================== //
//                                                                                          //
//  [3] Common Functions                                                                    //
//                                                                                          //
// ======================================================================================== //

// ================================================================ //
//   # Register Signal Handler                                      //
// ================================================================ //
    // 1. Common Register Signal Handler
    void common_register_signal_handler(void)
    {
        signal(SIGINT,  common_signal_stop_handler);
        signal(SIGKILL, common_xdma_signal_handler);
        signal(SIGQUIT, common_xdma_signal_handler);
        signal(SIGTERM, common_xdma_signal_handler);
        signal(SIGTSTP, common_xdma_signal_handler);
        signal(SIGHUP,  common_xdma_signal_handler);
        signal(SIGABRT, common_xdma_signal_handler);
    }

    // 2. Common Signal Stop Handler
    void common_signal_stop_handler(void)
    {
        if(watchStop) {
            common_xdma_signal_handler(2);
        } else {
            watchStop = 1;
        }
    }

    // 3. Common XDMA Signal Handler
    void common_xdma_signal_handler(int sig)
    {
        printf("\nXDMA-APP is exiting, cause (%d)!!\n", sig);
        sleep(1);
        app1_exit_task();
        printf("Tasks for Exit are Done!\n\n");
        sleep(1);
        exit(0);
    }



// ======================================================================================== //
//                                                                                          //
//  [3] Process Main Function's Definition                                                  //
//                                                                                          //
// ======================================================================================== //
    #define FRAME_TRANSMIT_TEST_RUN_OPTION_STRING  "s:"

    // 1. Process Main Function of Frame Transmission Test
    int process_main_frame_transmit_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl)
    {
        int tx_payload_size = 1024;
        int arg_flag;
    
        while((arg_flag = getopt(argc, (char**)argv, FRAME_TRANSMIT_TEST_RUN_OPTION_STRING)) != -1)
        {
            switch(arg_flag)
            {
                case 's' :
                    if(str2int(optarg, &tx_payload_size) != 0)
                    {
                        printf("Invalid parameter given or out of range for '-s'.");
                        return -1;
                    }
                    if ((tx_payload_size < 64) || (tx_payload_size > MAX_BUFFER_LENGTH))
                    {
                        printf("Data size %d is out of range.", tx_payload_size);
                        return -1;
                    }
    
                    break;
            }
        }

        return frame_tx_test_app(tx_payload_size);
    }
    
    // 2. Process Main Function of Frame Reception Test
    int process_main_frame_receive_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl)
    {
        return frame_rx_test_app();
    }
    
    // 3. Process Main Function of Register Read/Write Test
    int process_main_register_read_write_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl)
    {
        return register_rw_test_app();
    }

    int process_main_sel_tx_path_1_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl)
    {
        return sel_tx_path_1_test_app();
    }
    
    int process_main_sel_tx_path_2_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl)
    {
        return sel_tx_path_2_test_app();
    }

    int process_main_sel_rx_path_1_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl)
    {
        return sel_rx_path_1_test_app();
    }
    
    int process_main_sel_rx_path_2_testCmd(int argc, const char *argv[], menu_command_t *menu_tbl)
    {
        return sel_rx_path_2_test_app();
    }

// ======================================================================================== //
//                                                                                          //
//  [4] Main Command Table                                                                  //
//                                                                                          //
// ======================================================================================== //
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
        {"tx_test", EXECUTION_ATTR, process_main_frame_transmit_testCmd, \
            "   tx_test -s <size>", \
            "   This option was created for Frame Transmission Test of Re-Designed 1Q TSN IP\n"
            "   (Allowed Payload Size is between 64 and 2048)"},
        {"rx_test", EXECUTION_ATTR, process_main_frame_receive_testCmd, \
            "   rx_test", \
            "   This option was created for Frame Reception Test of Re-Designed 1Q TSN IP\n"},
        {"reg_test", EXECUTION_ATTR, process_main_register_read_write_testCmd, \
            "   reg_test", \
            "   This option was created for Register Read/Write Test of Re-Designed 1Q TSN IP\n"},
        {"sel_tx_path_1", EXECUTION_ATTR, process_main_sel_tx_path_1_testCmd, \
            "   reg_test", \
            "   This option was created for Register Read/Write Test of Re-Designed 1Q TSN IP\n"},
        {"sel_tx_path_2", EXECUTION_ATTR, process_main_sel_tx_path_2_testCmd, \
            "   reg_test", \
            "   This option was created for Register Read/Write Test of Re-Designed 1Q TSN IP\n"},
        {"sel_rx_path_1", EXECUTION_ATTR, process_main_sel_rx_path_1_testCmd, \
            "   reg_test", \
            "   This option was created for Register Read/Write Test of Re-Designed 1Q TSN IP\n"},
        {"sel_rx_path_2", EXECUTION_ATTR, process_main_sel_rx_path_2_testCmd, \
            "   reg_test", \
            "   This option was created for Register Read/Write Test of Re-Designed 1Q TSN IP\n"}, 
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


// ======================================================================================== //
//                                                                                          //
//  [5-1] Test App 1  :  Frame Transmission Test                                            //
//                                                                                          //
// ======================================================================================== //

// ================================================================ //
//   # Macro definition                                             //
// ================================================================ //
    // Constant Macro
    #define APP1_BUFFER_ALIGN               (4096)
    #define FRAME_TYPE_DATAPATH_VRFY_1      (0)
    #define FRAME_TYPE_DATAPATH_VRFY_2      (1)
    #define FRAME_TYPE_IEEE802              (2)
    #define FRAME_TYPE_VLAN                 (3)
    #define FRAME_TYPE_DIX2                 (4)
    #define FRAME_TYPE_JUMBO                (5)


    // User Parameter Macro
    #define APP1_SLEEP_TIME_MS              (10000000)                     // Frame Transmission Gap
    #define FRAME_TYPE                      (FRAME_TYPE_VLAN)        // Frame Type
    #define FRAME_LENGTH                    (1500)      // 46, 1456
    #define TIMESTMAP_ID                    (3)
    #define POLICY                          (1)

// ================================================================ //
//   # Variable definition                                          //
// ================================================================ //
    // 1. Structure for Frame Transmission
    typedef struct app1_frame_tx_arg {
        char devname[MAX_DEVICE_NAME];
        int size;
    } app1_frame_tx_arg_t;

    // 2. Pointer of Frame Transmission Buffer
    char*                   app1_buffer;

    // 3. H2C File Descriptor of XDMA
    int                     app1_xdma_h2c_fd;

    // 4. Frame Transmission Argument Structure
    app1_frame_tx_arg_t     app1_frame_tx_arg_structure;

    // 5. Stats of XDMA
    stats_t                 app1_xdma_stats;

    // 6. Frame Transmission Length ("2 * 128-bit" + "Payload Size")
    uint64_t                app1_frame_tx_length;

    // 7. Frame Transmission Index
    uint32_t                app1_frame_tx_index = 1;

    // 8. The Number of Byte Transmitted by the Prior XDMA Write Operation
    uint64_t                app1_bytes_transmitted;

    // 9. XDMA Write Status
    int                     app1_xdma_write_status;

    // 10. Sleep Time [ms]
    uint64_t                app1_sleep_time_ms = APP1_SLEEP_TIME_MS;

    // 11. XDMA Write Index
    uint64_t                app1_xdma_write_index = 1;

// ================================================================ //
//   # Frame Transmission Test App Function Definition              //
// ================================================================ //
    int frame_tx_test_app(int tx_payload_size)
    {
        // 1. Process Common Task for Test Application
        app1_init(tx_payload_size);

        // 2. Allocate Buffer for Tx
        app1_buffer_alloc(tx_payload_size);

        // 3. Construct Ethernet Frame to Transmit
        app1_tx_frame_construct();

        while(1)
        {
            printf("====== %lu[th] XDMA Write ======\n", app1_xdma_write_index++);

            // 1. XDMA Write Operation
            app1_xdma_write_status = xdma_api_write_from_buffer_with_fd(app1_frame_tx_arg_structure.devname, app1_xdma_h2c_fd, (char *)app1_buffer, app1_frame_tx_length, &app1_bytes_transmitted);
            if(app1_xdma_write_status != 0)
            {
                printf("XDMA Write : Failed\n");
            }
            else
            {
                printf("XDMA Write : Success\n");
            }

            printf("Byte Transmitted : %lu\n\n", app1_bytes_transmitted);

            usleep(app1_sleep_time_ms*1000);
        }
    }

// ================================================================ //
//   # Frame Transmission Test Helper Function                      //
// ================================================================ //
    // 1. Init Function of Frame Transmission Test App
    void app1_init(int tx_payload_size)
    {
        // 1. Initialize Structures
        memset(&app1_frame_tx_arg_structure, 0, sizeof(app1_frame_tx_arg_t));
        memset(&app1_xdma_stats, 0, sizeof(stats_t));
        
        // 2. Register Signal Handler
        common_register_signal_handler();

        // 3. Enable 1Q TSN Logic
        // TBD

        // 4. Config XDMA Device Name & Transmission Payload Size to the Frame Transmission Argument Structure
        memcpy(app1_frame_tx_arg_structure.devname, DEF_TX_DEVICE_NAME, sizeof(DEF_TX_DEVICE_NAME));
        app1_frame_tx_arg_structure.size = tx_payload_size;

        // 5. Open H2C Engine of XDMA & Get the File Descriptor of XDMA H2C
        if(xdma_api_dev_open(app1_frame_tx_arg_structure.devname, 0 /* eop_flush */, &app1_xdma_h2c_fd)) {
            printf("FAILURE: Could not open %s. Make sure xdma device driver is loaded and you have access rights (maybe use sudo?).\n", app1_frame_tx_arg_structure.devname);
            printf("<<< %s\n", __func__);
            return NULL;
        }
        else
        {
            printf("Open XDMA : Success\n\n");
        }
    }

    // 2. Buffer Allocation Function
    void app1_buffer_alloc(int tx_payload_size)
    {
        if(posix_memalign((void **)&app1_buffer, APP1_BUFFER_ALIGN /*alignment : 64 Bytes*/, (tx_payload_size*2)))
        {
            printf("Buffer allocation : Failed\n");
            return -1;
        }
        else
        {
            printf("Buffer allocation : Success\n");
            printf("Frame Transmission Buffer Address : %p\n", app1_buffer);
            printf("Buffer Mem Align : %d\n", APP1_BUFFER_ALIGN);
            memset(app1_buffer, 0, tx_payload_size);
        }
    }

    // 3. Construct Frame to Transmit
    void app1_tx_frame_construct(void)
    {
        struct tsn_tx_buffer*   app1_tx_buffer   = (struct tsn_tx_buffer*)app1_buffer;

    // ================================================================= //
    //                      Metadata1 Construnction                      //
    // ================================================================= //
        // Metadata1 Construction (=> From(64-bit) + To(64-bit) + Delay From(64-bit) + Delay To(64-bit))
        // uint32_t tick_from          = 0x11111111;
        // uint32_t tick_to            = 0x33333333;
        uint32_t tick_from          = 0;
        uint32_t tick_to            = (uint32_t)(0xFFFFFFFF);
        uint32_t tick_delay_from    = 0x55555555;
        uint32_t tick_delay_to      = 0x77777777;

        // 1. Tick From
        app1_buffer[0] = (uint8_t)(tick_from >> 24);
        app1_buffer[1] = (uint8_t)(tick_from >> 16);
        app1_buffer[2] = (uint8_t)(tick_from >> 8);
        app1_buffer[3] = (uint8_t)(tick_from);

        // 2. Tick To
        app1_buffer[4] = (uint8_t)(tick_to >> 24);
        app1_buffer[5] = (uint8_t)(tick_to >> 16);
        app1_buffer[6] = (uint8_t)(tick_to >> 8);
        app1_buffer[7] = (uint8_t)(tick_to);

        // 3. Tick Delay From
        app1_buffer[8] = (uint8_t)(tick_delay_from >> 24);
        app1_buffer[9] = (uint8_t)(tick_delay_from >> 16);
        app1_buffer[10] = (uint8_t)(tick_delay_from >> 8);
        app1_buffer[11] = (uint8_t)(tick_delay_from);

        // 4. Tick Delay To
        app1_buffer[12] = (uint8_t)(tick_delay_to >> 24);
        app1_buffer[13] = (uint8_t)(tick_delay_to >> 16);
        app1_buffer[14] = (uint8_t)(tick_delay_to >> 8);
        app1_buffer[15] = (uint8_t)(tick_delay_to);


    // ================================================================= //
    //                      Metadata2 Construnction                      //
    // ================================================================= //
        uint16_t frame_length = FRAME_LENGTH;
        uint16_t tstamp_id = TIMESTMAP_ID;
        uint8_t policy = POLICY;

        app1_buffer[16] = (uint8_t)(frame_length >> 8);
        app1_buffer[17] = (uint8_t)(frame_length);
        app1_buffer[18] = (uint8_t)(tstamp_id >> 8);
        app1_buffer[19] = (uint8_t)(tstamp_id);
        app1_buffer[20] = (uint8_t)(policy);

        app1_buffer[21] = (uint8_t)(0);
        app1_buffer[22] = (uint8_t)(0);
        app1_buffer[23] = (uint8_t)(0);

        app1_buffer[24] = (uint8_t)(0);
        app1_buffer[25] = (uint8_t)(0);
        app1_buffer[26] = (uint8_t)(0);
        app1_buffer[27] = (uint8_t)(0);
        app1_buffer[28] = (uint8_t)(0);
        app1_buffer[29] = (uint8_t)(0);
        app1_buffer[30] = (uint8_t)(0);
        app1_buffer[31] = (uint8_t)(0);

        // 3. Frame Construction
        uint8_t frame_type = FRAME_TYPE;

        switch(frame_type)
        {
            case FRAME_TYPE_DATAPATH_VRFY_1 :
                frame_type_datapath_vrfy_1(frame_length);
                break;

            case FRAME_TYPE_DATAPATH_VRFY_2 :
                frame_type_datapath_vrfy_2(frame_length);
                break;

            case FRAME_TYPE_IEEE802 :
                frame_type_ieee802(frame_length);
                break;

            case FRAME_TYPE_VLAN    :
                frame_type_vlan(frame_length);
                break;

            case FRAME_TYPE_DIX2    :
                frame_type_dix2(frame_length);
                break;

            case FRAME_TYPE_JUMBO   :
                frame_type_jumbo(frame_length);
                break;
        }

        // 4. Tx Frame Length
        app1_frame_tx_length = frame_length + 32;   // 32 : Metadata 1, Metadata2

        usleep(10*1000);
    }

    // 4. Test App1 Exit Process
    void app1_exit_task(void)
    {
        // Close XDMA device
        close(app1_xdma_h2c_fd);

        // Disable TEMAC & XDMA
        set_register(REG_TSN_CONTROL, 0);

        // Free buffer
        free(app1_buffer);
    }

    // 5. Frame Construction Type Function : For Datapath Verification 1
    void frame_type_datapath_vrfy_1(uint16_t frame_length)
    {
        // 1. Frame Construction
        for(uint16_t i = 0; i < frame_length; i++)
        {
            if(i % 16 == 0)
            {
                app1_buffer[32+i] = 0xd0;
            }
            else if(i % 16 == 15)
            {
                app1_buffer[32+i] = (uint8_t)(i / 16);
            }
            else
            {
                app1_buffer[32+i] = 0x00;
            }
        }
    }

    // 6. Frame Construction Type Function : For Datapath Verification 2
    void frame_type_datapath_vrfy_2(uint16_t frame_length)
    {
        for(uint16_t i = 0; i < frame_length; i++)
        {
            if(i % 6 == 0)
            {
                app1_buffer[32+i] = 0xAA;
            }
            else if(i % 6 == 1)
            {
                app1_buffer[32+i] = 0xBB;
            }
            else if(i % 6 == 2)
            {
                app1_buffer[32+i] = 0xCC;
            }
            else if(i % 6 == 3)
            {
                app1_buffer[32+i] = 0xDD;
            }
            else if(i % 6 == 4)
            {
                app1_buffer[32+i] = 0xEE;
            }
            else if(i % 6 == 5)
            {
                app1_buffer[32+i] = 0xFF;
            }
        }

        app1_buffer[32] = 0xbe;
        app1_buffer[33] = 0xef;
        app1_buffer[frame_length + 30] = 0xca;
        app1_buffer[frame_length + 31] = 0xfe;
    }

    // 7. Frame Construction Type Function : IEEE802.3 Frame
    void frame_type_ieee802(uint16_t frame_length)
    {
        // 1. DMAC
        app1_buffer[32] = 0xa1;
        app1_buffer[33] = 0xa2;
        app1_buffer[34] = 0xa3;
        app1_buffer[35] = 0xa4;
        app1_buffer[36] = 0xa5;
        app1_buffer[37] = 0xa6;

        // 2. SMAC
        app1_buffer[38] = 0xf1;
        app1_buffer[39] = 0xf2;
        app1_buffer[40] = 0xf3;
        app1_buffer[41] = 0xf4;
        app1_buffer[42] = 0xf5;
        app1_buffer[43] = 0xf6;

        // 3. LT_Upper
        app1_buffer[44] = 0x05;
        
        // 4. LT_Lower
        app1_buffer[45] = 0xdc;

        // 5. Payload
        for(uint16_t i = 0; i < (frame_length-14); i++)
        {
            // if(i % 6 == 0)
            // {
            //     app1_buffer[46+i] = 0xAA;
            // }
            // else if(i % 6 == 1)
            // {
            //     app1_buffer[46+i] = 0xBB;
            // }
            // else if(i % 6 == 2)
            // {
            //     app1_buffer[46+i] = 0xCC;
            // }
            // else if(i % 6 == 3)
            // {
            //     app1_buffer[46+i] = 0xDD;
            // }
            // else if(i % 6 == 4)
            // {
            //     app1_buffer[46+i] = 0xEE;
            // }
            // else if(i % 6 == 5)
            // {
            //     app1_buffer[46+i] = 0xFF;
            // }

            app1_buffer[46+i] = i % 256;
        }

        app1_buffer[frame_length + 30] = 0xca;
        app1_buffer[frame_length + 31] = 0xfe;
    }

    // 8. Frame Construction Type Function : VLAN Frame
    void frame_type_vlan(uint16_t frame_length)
    {
        // 1. DMAC
        app1_buffer[32] = 0xa1;
        app1_buffer[33] = 0xa2;
        app1_buffer[34] = 0xa3;
        app1_buffer[35] = 0xa4;
        app1_buffer[36] = 0xa5;
        app1_buffer[37] = 0xa6;

        // 2. SMAC
        app1_buffer[38] = 0xf1;
        app1_buffer[39] = 0xf2;
        app1_buffer[40] = 0xf3;
        app1_buffer[41] = 0xf4;
        app1_buffer[42] = 0xf5;
        app1_buffer[43] = 0xf6;

        // 3. VLAN Tag Upper/Lower
        app1_buffer[44] = 0x81;
        app1_buffer[45] = 0x00;

        // 4. LT Upper/Lower
        app1_buffer[46] = 0xFF;
        app1_buffer[47] = 0xFF;

        // 5. Payload
        for(uint16_t i = 0; i < (frame_length-14); i++)
        {
            // if(i % 6 == 0)
            // {
            //     app1_buffer[48+i] = 0xAA;
            // }
            // else if(i % 6 == 1)
            // {
            //     app1_buffer[48+i] = 0xBB;
            // }
            // else if(i % 6 == 2)
            // {
            //     app1_buffer[48+i] = 0xCC;
            // }
            // else if(i % 6 == 3)
            // {
            //     app1_buffer[48+i] = 0xDD;
            // }
            // else if(i % 6 == 4)
            // {
            //     app1_buffer[48+i] = 0xEE;
            // }
            // else if(i % 6 == 5)
            // {
            //     app1_buffer[48+i] = 0xFF;
            // }
            app1_buffer[48+i] = i % 256;
        }

        app1_buffer[frame_length + 30] = 0xca;
        app1_buffer[frame_length + 31] = 0xfe;
    }

    // 9. Frame Construction Type Function : DIX2.0 Frame (ARP Frame)
    void frame_type_dix2(uint16_t frame_length)
    {
        // 1. DMAC
        app1_buffer[32] = 0xa1;
        app1_buffer[33] = 0xa2;
        app1_buffer[34] = 0xa3;
        app1_buffer[35] = 0xa4;
        app1_buffer[36] = 0xa5;
        app1_buffer[37] = 0xa6;

        // 2. SMAC
        app1_buffer[38] = 0xf1;
        app1_buffer[39] = 0xf2;
        app1_buffer[40] = 0xf3;
        app1_buffer[41] = 0xf4;
        app1_buffer[42] = 0xf5;
        app1_buffer[43] = 0xf6;

        // 3. LT_Upper
        app1_buffer[44] = 0x08;
        
        // 4. LT_Lower
        app1_buffer[45] = 0x06;

        // 5. Payload
        for(uint16_t i = 0; i < (frame_length-14); i++)
        {
            // if(i % 6 == 0)
            // {
            //     app1_buffer[46+i] = 0xAA;
            // }
            // else if(i % 6 == 1)
            // {
            //     app1_buffer[46+i] = 0xBB;
            // }
            // else if(i % 6 == 2)
            // {
            //     app1_buffer[46+i] = 0xCC;
            // }
            // else if(i % 6 == 3)
            // {
            //     app1_buffer[46+i] = 0xDD;
            // }
            // else if(i % 6 == 4)
            // {
            //     app1_buffer[46+i] = 0xEE;
            // }
            // else if(i % 6 == 5)
            // {
            //     app1_buffer[46+i] = 0xFF;
            // }

            app1_buffer[46+i] = i % 256;
        }

        app1_buffer[frame_length + 30] = 0xca;
        app1_buffer[frame_length + 31] = 0xfe;
    }

    // 10. Frame Construction Type Function : Jumbo Frame (VLAN Frame)
    void frame_type_jumbo(uint16_t frame_length)
    {

    }


// ======================================================================================== //
//                                                                                          //
//  [4-2] Test App 2  :  Frame Reception Test                                               //
//                                                                                          //
// ======================================================================================== //

// ================================================================ //
//   # Macro definition                                             //
// ================================================================ //
    // Constant Macro
    #define APP2_BUFFER_ALIGN               (16)
    #define APP2_DATA_SIZE                  (16)

    // User Parameter Macro
    #define APP2_SLEEP_TIME_MS              (10000)


// ================================================================ //
//   # Variable definition                                          //
// ================================================================ //
    // 1. Structure for Frame Reception
    typedef struct app2_frame_rx_arg {
        char devname[MAX_DEVICE_NAME];
        int size;
    } app2_frame_rx_arg_t;

    // 2. Pointer of Frame Reception Buffer
    char*                   app2_buffer;

    // 3. C2H File Descriptor of XDMA
    int                     app2_xdma_c2h_fd;

    // 4. Frame Reception Argument Structure
    app2_frame_rx_arg_t     app2_frame_rx_arg_structure;

    // 5. Stats of XDMA
    stats_t                 app2_xdma_stats;

    // 6. The Number of Byte Received by the Prior XDMA Read Operation
    uint64_t                app2_bytes_received;

    // 7. XDMA Read Status
    int                     app2_xdma_read_status;

    // 8. Sleep Time [ms]
    uint64_t                app2_sleep_time_ms = APP2_SLEEP_TIME_MS;

    // 9. Rx Frame Index
    uint32_t                rx_frame_index = 1;

    // 9. Rx Frame 16-Byte Length
    uint16_t                rx_frame_16byte_length;

// ================================================================ //
//   # Frame Reception Test App Function Definition                 //
// ================================================================ //
    int frame_rx_test_app(void)
    {
        // 1. Process Common Task for Test Application
        app2_init();

        // 2. Allocate Buffer for Rx
        app2_buffer_alloc(APP2_DATA_SIZE);

        
        while(1)
        {
            app2_rx_meta_read();
            app2_rx_frame_read();         
        }
        
        return 0;
    }

// ================================================================ //
//   # Frame Reception Test Helper Function                         //
// ================================================================ //
    // 1. Init Function of Frame Reception Test App
    void app2_init(void)
    {
        // 1. Initialize Structures
        memset(&app2_frame_rx_arg_structure, 0, sizeof(app2_frame_rx_arg_t));
        memset(&app2_xdma_stats, 0, sizeof(stats_t));

        // 2. Register Signal Handler
        common_register_signal_handler();

        // 3. Config XDMA Device Name & Transmission Payload Size to the Frame Transmission Argument Structure
        memcpy(app2_frame_rx_arg_structure.devname, DEF_RX_DEVICE_NAME, sizeof(DEF_RX_DEVICE_NAME));

        // 4. Open H2C Engine of XDMA & Get the File Descriptor of XDMA H2C
        if(xdma_api_dev_open(app2_frame_rx_arg_structure.devname, 0 /* eop_flush */, &app2_xdma_c2h_fd)) {
            printf("FAILURE: Could not open %s. Make sure xdma device driver is loaded and you have access rights (maybe use sudo?).\n", app2_frame_rx_arg_structure.devname);
            printf("<<< %s\n", __func__);
            return NULL;
        }
        else
        {
            printf("Open XDMA : Success\n\n");
        }
    }

    // 2. Buffer Allocation Function
    void app2_buffer_alloc(int data_size)
    {
        if(posix_memalign((void **)&app2_buffer, APP2_BUFFER_ALIGN /* alignment : 16 Bytes */, data_size /* data_size : 16 Bytes (= 128bits) */))
        {
            printf("Buffer allocation : Failed\n");
            return -1;
        }
        else
        {
            printf("Buffer allocation : Success\n");
            printf("My buffer address : %p\n", app2_buffer);
            printf("Buffer Mem align : %d\n", APP2_BUFFER_ALIGN);
            memset(app2_buffer, 0, data_size);
        }
    }

    // 3. XDMA Read Function
    void app2_xdma_read(void)
    {
        app2_xdma_read_status = xdma_api_read_to_buffer_with_fd(app2_frame_rx_arg_structure.devname, app2_xdma_c2h_fd, (char *)app2_buffer, APP2_DATA_SIZE, &app2_bytes_received);

        if(app2_xdma_read_status == 0)
        {
            // printf("XDMA Read : Success\n");
        }
        else
        {
            printf("XDMA Read : Failed\n");
        }
    }

    // 4. Rx Metadata Read Function
    void app2_rx_meta_read(void)
    {
        uint64_t rx_tstamp = 0;
        uint16_t rx_frame_length = 0;
        uint16_t rx_vlan_tag = 0;
        uint64_t rx_dmac = 0, rx_smac = 0;
        uint16_t rx_ether_type = 0;

        printf("\n\n\n========================= %luth Rx Frame =========================\n\n", rx_frame_index++);

        // ================================================================ //
        //   # First 16-Byte Read                                           //
        // ================================================================ //
        // 1. Read 1st 16-Byte
        app2_xdma_read();

        for(int i = 0; i < APP2_DATA_SIZE; i++)
        {
            printf("%02X ", (unsigned char)app2_buffer[i]);
        }
        printf("  => Rx Byte : %d", app2_bytes_received);
        printf("\n");

        // 2. Get Rx Tstamp
        for(int i = 0; i <= 7; i++)
        {
            rx_tstamp = rx_tstamp | ((uint64_t)app2_buffer[i] << 8*(7-i));
        }

        // 3. Get Frame Length
        rx_frame_length = (uint16_t)(((uint16_t)app2_buffer[8] << 8) | (uint16_t)app2_buffer[9]);

        // 4. Get DMAC
        for(int i = 10; i <= 15; i++)
        {
            rx_dmac = rx_dmac | ((uint64_t)app2_buffer[i] << 8*(15-i));
        }

        // 5. Calculate 16-Byte Frame Length
        if(rx_frame_length % 16 == 0)
        {
            rx_frame_16byte_length = rx_frame_length / 16;
        }
        else
        {
            rx_frame_16byte_length = (rx_frame_length / 16) + 1;
        }

        // 6. Print Rx Frame Information
        printf("Rx Tstamp       : %016llX\n", rx_tstamp);
        printf("Rx Frame Length : %d\n", rx_frame_length);
        printf("Rx DMAC         : %012llX\n", rx_dmac);
        printf("\n");

        // ================================================================ //
        //   # Second 16-Byte Read                                          //
        // ================================================================ //
        // 1. Read 2nd 16-Byte
        app2_xdma_read();

        for(int i = 0; i < APP2_DATA_SIZE; i++)
        {
            printf("%02X ", (unsigned char)app2_buffer[i]);
        }
        printf("  => Rx Byte : %d", app2_bytes_received);
        printf("\n");

        // 2. Get SMAC
        for(int i = 0; i <= 5; i++)
        {
            rx_smac = rx_smac | ((uint64_t)app2_buffer[i] << 8*(5-i));
        }

        // 3. Get Ether-Type
        rx_ether_type = (uint16_t)(((uint16_t)app2_buffer[6] << 8) | (uint16_t)app2_buffer[7]);

        // 4. Print Rx Frame Information
        printf("Rx SMAC         : %012llX\n", rx_smac);
        printf("Rx Ether-Type   : %04X\n", rx_ether_type);
        printf("\n");
    }

    // 5. Rx Frame Data Read Function
    void app2_rx_frame_read(void)
    {
        printf("// Frame Data (Payload)\n");

        // for(int i = 0; i < rx_frame_16byte_length - 1; i++)
        // {
        //     app2_xdma_read();

        //     for(int j = 0; j < APP2_DATA_SIZE; j++)
        //     {
        //         printf("%02X ", (unsigned char)app2_buffer[j]);
        //     }
        //     printf("  => Rx Byte : %d", app2_bytes_received);
        //     printf("\n");

        //     app2_bytes_received = 0;
        //     usleep(20*1000);
        // }

        for(int i = 0; i < rx_frame_16byte_length; i++)
        {
            app2_xdma_read();

            for(int i = 0; i < APP2_DATA_SIZE; i++)
            {
                printf("%02X ", (unsigned char)app2_buffer[i]);
            }
            printf("  => Rx Byte : %d", app2_bytes_received);
            printf("\n");

            app2_bytes_received = 0;
            usleep(20*1000);
        }
    }


// ======================================================================================== //
//                                                                                          //
//  [4-3] Test App 3  :  Register Read / Write Test                                         //
//                                                                                          //
// ======================================================================================== //

// ================================================================ //
//   # Macro definition                                             //
// ================================================================ //


// ================================================================ //
//   # Variable definition                                          //
// ================================================================ //


// ================================================================ //
//   # Register Read / Write Test App Function Definition           //
// ================================================================ //
    int register_rw_test_app(void)
    {
        // 1. General System Info Register
        uint32_t rd_reg_11th_hi, rd_reg_11th_lo;        // FPGA Logic Version
        uint32_t rd_reg_13th_hi, rd_reg_13th_lo;        // FPGA Running Time
        uint32_t rd_reg_12th_hi, rd_reg_12th_lo;        // System Count

        // 2. TSN Tx Info Register
        uint32_t rd_reg_79th_hi, rd_reg_79th_lo;        // ADDR FIFO Data Count
        uint32_t rd_reg_91th_hi, rd_reg_91th_lo;        // Tx Tstamp1
        uint32_t rd_reg_92th_hi, rd_reg_92th_lo;        // Tx Tstamp2
        uint32_t rd_reg_93th_hi, rd_reg_93th_lo;        // Tx Tstamp3
        uint32_t rd_reg_94th_hi, rd_reg_94th_lo;        // Tx Tstamp4

        uint32_t rd_reg_56th_hi, rd_reg_56th_lo;        // Total Rx Frame Count
        uint32_t rd_reg_58th_hi, rd_reg_58th_lo;        // Total Back Pressure Event Count

        uint32_t rd_reg_70th_hi, rd_reg_70th_lo;        // FSCH Total New Entry Count
        uint32_t rd_reg_71th_hi, rd_reg_71th_lo;        // FSCH Total Valid Entry Count
        uint32_t rd_reg_72th_hi, rd_reg_72th_lo;        // FSCH Total Delay Entry Count
        uint32_t rd_reg_73th_hi, rd_reg_73th_lo;        // FSCH Total Drop Entry Count

        uint32_t rd_reg_97th_hi, rd_reg_97th_lo;        // FT1 Total Tx Frame Count
        uint32_t rd_reg_98th_hi, rd_reg_98th_lo;        // FT1 Total Tx Byte Count
        uint32_t rd_reg_103th_hi, rd_reg_103th_lo;      // FT2 Total Tx Frame Count
        uint32_t rd_reg_104th_hi, rd_reg_104th_lo;      // FT2 Total Tx Byte Count

        // 3. TSN Rx Info Register
        // 3-1. ETH Port 1
        uint32_t rd_reg_16th_hi, rd_reg_16th_lo;        // ETH1 Rx Timestamp
        uint32_t rd_reg_19th_hi, rd_reg_19th_lo;        // ETH1 Total Rx Frame Count
        uint32_t rd_reg_20th_hi, rd_reg_20th_lo;        // ETH1 Total Rx Byte Count
        uint32_t rd_reg_21th_hi, rd_reg_21th_lo;        // ETH1 Total Drop Frame Count
        uint32_t rd_reg_22th_hi, rd_reg_22th_lo;        // ETH1 Total Drop Byte Count
        uint32_t rd_reg_31th_hi, rd_reg_31th_lo;        // Rx FIFO Status

        // 3-2. ETH Port 2
        uint32_t rd_reg_35th_hi, rd_reg_35th_lo;        // ETH1 Rx Timestamp
        uint32_t rd_reg_38th_hi, rd_reg_38th_lo;        // ETH1 Total Rx Frame Count
        uint32_t rd_reg_39th_hi, rd_reg_39th_lo;        // ETH1 Total Rx Byte Count
        uint32_t rd_reg_40th_hi, rd_reg_40th_lo;        // ETH1 Total Drop Frame Count
        uint32_t rd_reg_41th_hi, rd_reg_41th_lo;        // ETH1 Total Drop Byte Count
        uint32_t rd_reg_50th_hi, rd_reg_50th_lo;        // Rx FIFO Status

        // Test
        uint32_t rd_reg_1th_hi, rd_reg_1th_lo;


        usleep(10*1000);


        while(1)
        {
            printf("===========================================================================\n");
            printf("                   1Q TSN Logic's Internal Status Register                 \n");
            printf("===========================================================================\n");

        // ==================================================================== //
        //  [1] TSN General System Information                                  //
        // ==================================================================== //
            rd_reg_11th_hi = get_register(REG_11TH_HIGH);   // FPGA Logic Version
            rd_reg_11th_lo = get_register(REG_11TH_LOW);    // FPGA Logic Version
            rd_reg_13th_hi = get_register(REG_13TH_HIGH);   // FPGA Running Time
            rd_reg_13th_lo = get_register(REG_13TH_LOW);    // FPGA Running Time
            rd_reg_12th_hi = get_register(REG_12TH_HIGH);   // System Count
            rd_reg_12th_lo = get_register(REG_12TH_LOW);    // System Count

            printf(" # FPGA Running Time => %02lu : %02lu : %02lu\n", ((rd_reg_13th_lo >> 16U) & 0x7F), ((rd_reg_13th_lo >> 8U) & 0xFF), (rd_reg_13th_lo & 0xFF));
            printf("\n");

            printf(" # 1Q TSN SYSTEM INFO\n");
            printf("      FPGA Logic Version         :  %lu.%lu.%lu (Ver : %lu)\n", ((rd_reg_11th_lo >> 24U) & 0xFF), ((rd_reg_11th_lo >> 16U) & 0xFF), ((rd_reg_11th_lo >> 8U) & 0xFF), (rd_reg_11th_lo & 0xFF));
            printf("      System Count (Host) (hi)   :  %lu\n", rd_reg_12th_hi);
            printf("      System Count (Host) (lo)   :  %lu\n", rd_reg_12th_lo);
            printf("\n\n");
            
        // ==================================================================== //
        //  [2] TSN Tx Information                                              //
        // ==================================================================== //
            printf(" # 1Q TSN TX INFO\n");
            printf("   [1] COMMON\n");

            // Address FIFO Data Count
            rd_reg_79th_hi = get_register(REG_79TH_HIGH);
            rd_reg_79th_lo = get_register(REG_79TH_LOW);

            printf("       Address FIFO Data Count (hi)  :  %lu\n", rd_reg_79th_hi);
            printf("       Address FIFO Data Count (lo)  :  %lu\n", rd_reg_79th_lo);
            printf("\n");

            // FS Total Rx Frame Count
            rd_reg_56th_hi = get_register(REG_56TH_HIGH);   // FS Total Rx Frame Count (Upper)
            rd_reg_56th_lo = get_register(REG_56TH_LOW);    // FS Total Rx Frame Count (Lower)
            rd_reg_58th_hi = get_register(REG_58TH_HIGH);   // FS Total Back-Pressure Event Count (Upper)
            rd_reg_58th_lo = get_register(REG_58TH_LOW);    // FS Total Back-Pressure Event Count (Lower)

            printf("       FS Total Rx Frame Count (hi)                :  %lu\n", rd_reg_56th_hi);
            printf("       FS Total Rx Frame Count (lo)                :  %lu\n", rd_reg_56th_lo);
            printf("       FS Total Back-Pressure Event Count (hi)     :  %lu\n", rd_reg_58th_lo);
            printf("       FS Total Back-Pressure Event Count (lo)     :  %lu\n", rd_reg_58th_lo);
            printf("\n");

            // FSCH Total New/Valid/Delay/Drop Entry Count
            rd_reg_70th_hi = get_register(REG_70TH_HIGH);   // FSCH Total New Entry Count   (Upper)
            rd_reg_70th_lo = get_register(REG_70TH_LOW);    // FSCH Total New Entry Count   (Lower)
            rd_reg_71th_hi = get_register(REG_71TH_HIGH);   // FSCH Total Valid Entry Count (Upper)
            rd_reg_71th_lo = get_register(REG_71TH_LOW);    // FSCH Total Valid Entry Count (Lower)
            rd_reg_72th_hi = get_register(REG_72TH_HIGH);   // FSCH Total Delay Entry Count (Upper)
            rd_reg_72th_lo = get_register(REG_72TH_LOW);    // FSCH Total Delay Entry Count (Lower)
            rd_reg_73th_hi = get_register(REG_73TH_HIGH);   // FSCH Total Drop Entry Count  (Upper)
            rd_reg_73th_lo = get_register(REG_73TH_LOW);    // FSCH Total Drop Entry Count  (Lower)

            printf("       FSCH Total New Entry Count   (hi)  :  %lu\n", rd_reg_70th_hi);
            printf("       FSCH Total New Entry Count   (lo)  :  %lu\n", rd_reg_70th_lo);
            printf("       FSCH Total Valid Entry Count (hi)  :  %lu\n", rd_reg_71th_hi);
            printf("       FSCH Total Valid Entry Count (lo)  :  %lu\n", rd_reg_71th_lo);
            printf("       FSCH Total Delay Entry Count (hi)  :  %lu\n", rd_reg_72th_hi);
            printf("       FSCH Total Delay Entry Count (lo)  :  %lu\n", rd_reg_72th_lo);
            printf("       FSCH Total Drop Entry Count  (hi)  :  %lu\n", rd_reg_73th_hi);
            printf("       FSCH Total Drop Entry Count  (lo)  :  %lu\n", rd_reg_73th_lo);
            printf("\n");

            // Tx Tstamp
            rd_reg_91th_hi = get_register(REG_91TH_HIGH);   // Tx Tstamp 1 (Upper)
            rd_reg_91th_lo = get_register(REG_91TH_LOW);    // Tx Tstamp 1 (Lower)
            rd_reg_92th_hi = get_register(REG_92TH_HIGH);   // Tx Tstamp 2 (Upper)
            rd_reg_92th_lo = get_register(REG_92TH_LOW);    // Tx Tstamp 2 (Lower)
            rd_reg_93th_hi = get_register(REG_93TH_HIGH);   // Tx Tstamp 3 (Upper)
            rd_reg_93th_lo = get_register(REG_93TH_LOW);    // Tx Tstamp 3 (Lower)
            rd_reg_94th_hi = get_register(REG_94TH_HIGH);   // Tx Tstamp 4 (Upper)
            rd_reg_94th_lo = get_register(REG_94TH_LOW);    // Tx Tstamp 4 (Lower)

            printf("       Tx Tstamp 1 (hi)     :  %lu\n", rd_reg_91th_hi);
            printf("       Tx Tstamp 1 (lo)     :  %lu\n", rd_reg_91th_lo);
            printf("       Tx Tstamp 2 (hi)     :  %lu\n", rd_reg_92th_hi);
            printf("       Tx Tstamp 2 (lo)     :  %lu\n", rd_reg_92th_lo);
            printf("       Tx Tstamp 3 (hi)     :  %lu\n", rd_reg_93th_hi);
            printf("       Tx Tstamp 3 (lo)     :  %lu\n", rd_reg_93th_lo);
            printf("       Tx Tstamp 4 (hi)     :  %lu\n", rd_reg_94th_hi);
            printf("       Tx Tstamp 4 (lo)     :  %lu\n", rd_reg_94th_lo);
            printf("\n");

            printf("   [2] ETH PORT 1\n");
            // FT1 Total Tx Frame/Byte Count
            rd_reg_97th_hi = get_register(REG_97TH_HIGH);   // FT1 Total Tx Frame Count   (Upper)
            rd_reg_97th_lo = get_register(REG_97TH_LOW);    // FT1 Total Tx Frame Count   (Lower)
            rd_reg_98th_hi = get_register(REG_98TH_HIGH);   // FT1 Total Tx Byte Count    (Upper)
            rd_reg_98th_lo = get_register(REG_98TH_LOW);    // FT1 Total Tx Byte Count    (Lower)

            printf("       FT1 Total Tx Frame Count   (hi)  :  %lu\n", rd_reg_97th_hi);
            printf("       FT1 Total Tx Frame Count   (lo)  :  %lu\n", rd_reg_97th_lo);
            printf("       FT1 Total Tx Byte Count    (hi)  :  %lu\n", rd_reg_98th_hi);
            printf("       FT1 Total Tx Byte Count    (lo)  :  %lu\n", rd_reg_98th_lo);
            printf("\n");

            printf("   [3] ETH PORT 2\n");
            // FT2 Total Tx Frame/Byte Count
            rd_reg_103th_hi = get_register(REG_103TH_HIGH);   // FT2 Total Tx Frame Count   (Upper)
            rd_reg_103th_lo = get_register(REG_103TH_LOW);    // FT2 Total Tx Frame Count   (Lower)
            rd_reg_104th_hi = get_register(REG_104TH_HIGH);   // FT2 Total Tx Byte Count    (Upper)
            rd_reg_104th_lo = get_register(REG_104TH_LOW);    // FT2 Total Tx Byte Count    (Lower)

            printf("       FT2 Total Tx Frame Count   (hi)  :  %lu\n", rd_reg_103th_hi);
            printf("       FT2 Total Tx Frame Count   (lo)  :  %lu\n", rd_reg_103th_lo);
            printf("       FT2 Total Tx Byte Count    (hi)  :  %lu\n", rd_reg_104th_hi);
            printf("       FT2 Total Tx Byte Count    (lo)  :  %lu\n", rd_reg_104th_lo);
            printf("\n\n");

        // ==================================================================== //
        //  [3] TSN Rx Information                                              //
        // ==================================================================== //
            printf(" # 1Q TSN RX INFO\n");
            printf("   [1] ETH PORT 1\n");

            // Rx Tstamp
            rd_reg_16th_hi = get_register(REG_16TH_HIGH);   // Rx Tstamp (Upper)
            rd_reg_16th_lo = get_register(REG_16TH_LOW);    // Rx Tstamp (Lower)

            printf("       Rx Tstamp (hi)                     :  %lu\n", rd_reg_16th_hi);
            printf("       Rx Tstamp (lo)                     :  %lu\n", rd_reg_16th_lo);
            printf("\n");

            // FD Total Rx Frame Count
            rd_reg_19th_hi = get_register(REG_19TH_HIGH);   // FD Total Rx Frame Count (Upper)
            rd_reg_19th_lo = get_register(REG_19TH_LOW);    // FD Total Rx Frame Count (Lower)

            printf("       FD Total Rx Frame Count (hi)       :  %lu\n", rd_reg_19th_hi);
            printf("       FD Total Rx Frame Count (lo)       :  %lu\n", rd_reg_19th_lo);

            // FD Total Rx Byte Count
            rd_reg_20th_hi = get_register(REG_20TH_HIGH);   // FD Total Rx Byte Count (Upper)
            rd_reg_20th_lo = get_register(REG_20TH_LOW);    // FD Total Rx Byte Count (Lower)

            printf("       FD Total Rx Byte Count (hi)        :  %lu\n", rd_reg_20th_hi);
            printf("       FD Total Rx Byte Count (lo)        :  %lu\n", rd_reg_20th_lo);

            // FD Total Rx Drop Frame Count
            rd_reg_21th_hi = get_register(REG_21TH_HIGH);   // FD Total Rx Drop Frame Count (Upper)
            rd_reg_21th_lo = get_register(REG_21TH_LOW);    // FD Total Rx Drop Frame Count (Lower)

            printf("       FD Total Rx Drop Frame Count (hi)  :  %lu\n", rd_reg_21th_hi);
            printf("       FD Total Rx Drop Frame Count (lo)  :  %lu\n", rd_reg_21th_lo);

            // FD Total Rx Drop Byte Count
            rd_reg_22th_hi = get_register(REG_22TH_HIGH);   // FD Total Rx Drop Byte Count (Upper)
            rd_reg_22th_lo = get_register(REG_22TH_LOW);    // FD Total Rx Drop Byte Count (Lower)

            printf("       FD Total Rx Drop Byte Count (hi)   :  %lu\n", rd_reg_22th_hi);
            printf("       FD Total Rx Drop Byte Count (lo)   :  %lu\n", rd_reg_22th_lo);
            printf("\n");

            // Rx Host FIFO, Rx Frame FIFO, Rx Meta FIFO Data Count
            rd_reg_31th_hi = get_register(REG_31TH_HIGH);   // Rx FIFO Status (Upper)
            rd_reg_31th_lo = get_register(REG_31TH_LOW);    // Rx FIFO Status (Lower)

            printf("       Rx Host FIFO Data Count            :  %lu (%lu-Byte)\n", rd_reg_31th_hi, rd_reg_31th_hi*16);
            printf("       Rx Frame FIFO Data Count           :  %lu\n", (rd_reg_31th_lo >> 16U));
            printf("       Rx Meta FIFO Data Count            :  %lu\n", (rd_reg_31th_lo & 0xFF));
            printf("\n");

            printf("   [2] ETH PORT 2\n");

            // Rx Tstamp
            rd_reg_35th_hi = get_register(REG_35TH_HIGH);   // Rx Tstamp (Upper)
            rd_reg_35th_lo = get_register(REG_35TH_LOW);    // Rx Tstamp (Lower)

            printf("       Rx Tstamp (hi)                     :  %lu\n", rd_reg_35th_hi);
            printf("       Rx Tstamp (lo)                     :  %lu\n", rd_reg_35th_lo);
            printf("\n");

            // FD Total Rx Frame Count
            rd_reg_38th_hi = get_register(REG_38TH_HIGH);   // FD Total Rx Frame Count (Upper)
            rd_reg_38th_lo = get_register(REG_38TH_LOW);    // FD Total Rx Frame Count (Lower)

            printf("       FD Total Rx Frame Count (hi)       :  %lu\n", rd_reg_38th_hi);
            printf("       FD Total Rx Frame Count (lo)       :  %lu\n", rd_reg_38th_lo);

            // FD Total Rx Byte Count
            rd_reg_39th_hi = get_register(REG_39TH_HIGH);   // FD Total Rx Byte Count (Upper)
            rd_reg_39th_lo = get_register(REG_39TH_LOW);    // FD Total Rx Byte Count (Lower)

            printf("       FD Total Rx Byte Count (hi)        :  %lu\n", rd_reg_39th_hi);
            printf("       FD Total Rx Byte Count (lo)        :  %lu\n", rd_reg_39th_lo);

            // FD Total Rx Drop Frame Count
            rd_reg_40th_hi = get_register(REG_40TH_HIGH);   // FD Total Rx Drop Frame Count (Upper)
            rd_reg_40th_lo = get_register(REG_40TH_LOW);    // FD Total Rx Drop Frame Count (Lower)

            printf("       FD Total Rx Drop Frame Count (hi)  :  %lu\n", rd_reg_40th_hi);
            printf("       FD Total Rx Drop Frame Count (lo)  :  %lu\n", rd_reg_40th_lo);

            // FD Total Rx Drop Byte Count
            rd_reg_41th_hi = get_register(REG_41TH_HIGH);   // FD Total Rx Drop Byte Count (Upper)
            rd_reg_41th_lo = get_register(REG_41TH_LOW);    // FD Total Rx Drop Byte Count (Lower)

            printf("       FD Total Rx Drop Byte Count (hi)   :  %lu\n", rd_reg_41th_hi);
            printf("       FD Total Rx Drop Byte Count (lo)   :  %lu\n", rd_reg_41th_lo);
            printf("\n");

            // Rx Host FIFO, Rx Frame FIFO, Rx Meta FIFO Data Count
            rd_reg_50th_hi = get_register(REG_50TH_HIGH);   // Rx FIFO Status (Upper)
            rd_reg_50th_lo = get_register(REG_50TH_LOW);    // Rx FIFO Status (Lower)

            printf("       Rx Host FIFO Data Count            :  %lu (%lu-Byte)\n", rd_reg_50th_hi, rd_reg_50th_hi*16);
            printf("       Rx Frame FIFO Data Count           :  %lu\n", (rd_reg_50th_lo >> 16U));
            printf("       Rx Meta FIFO Data Count            :  %lu\n", (rd_reg_50th_lo & 0xFF));
            printf("\n");

            // Test
            // rd_reg_1th_hi = get_register(REG_1TH_HIGH);
            // rd_reg_1th_lo = get_register(REG_1TH_LOW);

            // printf("       TSN System Control (hi)            :  %lx\n", rd_reg_1th_hi);
            // printf("       TSN System Control (lo)            :  %lu\n", rd_reg_1th_lo);
            // printf("\n");

            usleep(100*1000);
        }
        

        return 0;
    }

    // int register_rw_test_app(void)
    // {
    //     // 1. General System Information
    //     uint32_t up_count_high, up_count_low;
        
    //     up_count_high   = get_register(TEST_REG_1TH_HIGH);   // Up Count High
    //     up_count_low    = get_register(TEST_REG_1TH_LOW);    // Up Count Low

    //     printf("   1. Up Count      (hi)  :  %lu\n", up_count_high);
    //     printf("      Up Count      (lo)  :  %lu\n\n", up_count_low);

    //     set_register(REG_87TH_HIGH, 0xdeadbeef);
    //     set_register(REG_87TH_LOW, 0xcafebabe);

    //     usleep(100*1000);

    //     return 0;
    // }

    // int register_rw_test_app(void)
    // {
    //     // 1. General System Information
    //     uint32_t up_count_high, up_count_low;
    //     uint32_t down_count_high, down_count_low;
    //     uint32_t time_hour, time_minute;
    //     uint32_t time_second, time_tick;
    //     usleep(10*1000);

    //     while(1)
    //     {
    //         printf("=========================== Register Information ===========================\n");

    //     // ==================================================================== //
    //     //  [1] System Information                                  //
    //     // ==================================================================== //
    //         up_count_high   = get_register(TEST_REG_1TH_HIGH);   // Up Count High
    //         up_count_low    = get_register(TEST_REG_1TH_LOW);    // Up Count Low
    //         down_count_high = get_register(TEST_REG_2TH_HIGH);   // Down Count High
    //         down_count_low  = get_register(TEST_REG_2TH_LOW);    // Down Count Low
    //         time_hour       = get_register(TEST_REG_3TH_HIGH);   // Time Hour
    //         time_minute     = get_register(TEST_REG_3TH_LOW);    // Time Minute
    //         time_second     = get_register(TEST_REG_4TH_HIGH);   // Time Second
    //         time_tick       = get_register(TEST_REG_4TH_LOW);    // Time Tick

    //         printf("FPGA Running Time => %02lu : %02lu : %02lu\n\n", time_hour, time_minute, time_second);
    //         printf("// General System Information\n");
    //         printf("   1. Up Count      (hi)  :  %lu\n", up_count_high);
    //         printf("      Up Count      (lo)  :  %lu\n", up_count_low);
    //         printf("\n");
    //         printf("   2. Down Count    (hi)  :  %lu\n", down_count_high);
    //         printf("      Down Count    (lo)  :  %lu\n", down_count_low);
    //         printf("\n\n\n\n");
            
    //         usleep(100*1000);
    //     }
        
    //     return 0;
    // }


    // int register_rw_test_app(void)
    // {
    //     // 1. General System Information
    //     uint32_t rd_reg_80th_hi, rd_reg_80th_lo;    // TSN System Info
    //     uint32_t rd_reg_76th_hi, rd_reg_76th_lo;    // FPGA Clock (Hour), FPGA Clock (Minute)
    //     uint32_t rd_reg_77th_hi, rd_reg_77th_lo;    // FPGA Clock (Second), FPGA Clock (Tick)
    //     uint32_t rd_reg_82th_hi, rd_reg_82th_lo;    // System Count (Host)
    //     uint32_t rd_reg_78th_hi, rd_reg_78th_lo;    // System Count (TSN Rx)
    //     uint32_t rd_reg_79th_hi, rd_reg_79th_lo;    // System Count (TSN Tx)

    //     // 2. TSN Tx Information
    //     uint32_t rd_reg_42th_hi, rd_reg_42th_lo;    // Buffer Write Status 1 (Address FIFO Data Count)
    //     uint32_t rd_reg_45th_hi, rd_reg_45th_lo;    // Address FIFO Data Count
    //     uint32_t rd_reg_64th_hi, rd_reg_64th_lo;    // Tx Tstamp 1
    //     uint32_t rd_reg_65th_hi, rd_reg_65th_lo;    // Tx Tstamp 2
    //     uint32_t rd_reg_66th_hi, rd_reg_66th_lo;    // Tx Tstamp 3
    //     uint32_t rd_reg_67th_hi, rd_reg_67th_lo;    // Tx Tstamp 4
    //     uint32_t rd_reg_24th_hi, rd_reg_24th_lo;    // FS Total Rx Frame Count
    //     uint32_t rd_reg_37th_hi, rd_reg_37th_lo;    // FSCH Total New Entry Count
    //     uint32_t rd_reg_38th_hi, rd_reg_38th_lo;    // FSCH Total Valid Entry Count
    //     uint32_t rd_reg_39th_hi, rd_reg_39th_lo;    // FSCH Total Delay Entry Count
    //     uint32_t rd_reg_40th_hi, rd_reg_40th_lo;    // FSCH Total Drop Entry Count
    //     uint32_t rd_reg_58th_hi, rd_reg_58th_lo;    // FT Total Tx Frame Count
    //     uint32_t rd_reg_59th_hi, rd_reg_59th_lo;    // FT Total Tx Byte Count

    //     // 3. TSN Rx Information
    //     uint32_t rd_reg_1th_hi, rd_reg_1th_lo;      // Rx Tstamp
    //     uint32_t rd_reg_4th_hi, rd_reg_4th_lo;      // Total Rx Frame Count


    //     usleep(10*1000);

    //     // set_register(REG_84TH_HIGH, (uint32_t)(900 << 16 | 6195));
    //     // // set_register(REG_84TH_LOW, (uint32_t)(900 << 16 | 6195));
    //     // set_register(REG_84TH_LOW, (uint32_t)(510 << 16 | 1 << 7 | 1 << 6 | 1 << 5 | 1 << 4 | 1 << 3 | 1 << 2 | 1 << 1 | 1));

    //     set_register(REG_86TH_HIGH, 0xdeadbeef);
    //     set_register(REG_86TH_LOW, 0xcafebabe);

    //     // set_register(REG_86TH_HIGH, 0x0);
    //     // set_register(REG_86TH_LOW, 0x0);

    //     // set_register(REG_87TH_HIGH, 0xdeadbeef);
    //     // set_register(REG_87TH_LOW, 0xcafebabe);

    //     // set_register(REG_87TH_HIGH, 0x0);
    //     // set_register(REG_87TH_LOW, 0x0);



    //     while(1)
    //     {
    //         printf("=========================== Register Information ===========================\n");

    //     // ==================================================================== //
    //     //  [1] TSN General System Information                                  //
    //     // ==================================================================== //
    //         rd_reg_80th_hi = get_register(REG_80TH_HIGH);   // TSN System Info
    //         rd_reg_80th_lo = get_register(REG_80TH_LOW);    // TSN System Info
    //         rd_reg_76th_hi = get_register(REG_76TH_HIGH);   // FPGA Clock (Hour)
    //         rd_reg_76th_lo = get_register(REG_76TH_LOW);    // FPGA Clock (Minute)
    //         rd_reg_77th_hi = get_register(REG_77TH_HIGH);   // FPGA Clock (Second)
    //         rd_reg_77th_lo = get_register(REG_77TH_LOW);    // FPGA Clock (Tick)
    //         rd_reg_82th_hi = get_register(REG_82TH_HIGH);   // System Count (Host)
    //         rd_reg_82th_lo = get_register(REG_82TH_LOW);    // System Count (Host)
    //         rd_reg_78th_hi = get_register(REG_78TH_HIGH);   // System Count (TSN Rx)
    //         rd_reg_78th_lo = get_register(REG_78TH_LOW);    // System Count (TSN Rx)
    //         rd_reg_79th_hi = get_register(REG_79TH_HIGH);   // System Count (TSN Tx)
    //         rd_reg_79th_lo = get_register(REG_79TH_LOW);    // System Count (TSN Tx)

    //         printf("FPGA Running Time => %02lu : %02lu : %02lu\n\n", rd_reg_76th_hi, rd_reg_76th_lo, rd_reg_77th_hi);
    //         printf("// General System Information\n");
    //         printf("   1. System Count (Host) (hi)     : %lu\n", rd_reg_82th_hi);
    //         printf("      System Count (Host) (lo)     : %lu\n", rd_reg_82th_lo);
    //         printf("\n\n");

    //     // ==================================================================== //
    //     //  [2] TSN Tx Information                                              //
    //     // ==================================================================== //
    //         printf("// TSN Tx Information\n");

    //         // Buffer Write Status 1 (Address FIFO Data Count)
    //         rd_reg_42th_hi = get_register(REG_42TH_HIGH);
    //         rd_reg_42th_lo = get_register(REG_42TH_LOW);

    //         printf("   1. Buffer Write Status 1 (hi)  : %lu\n", rd_reg_42th_hi);
    //         printf("      Buffer Write Status 1 (lo)  : %lu\n", rd_reg_42th_lo);

    //         // Address FIFO Data Count
    //         rd_reg_45th_hi = get_register(REG_45TH_HIGH);
    //         rd_reg_45th_lo = get_register(REG_45TH_LOW);

    //         printf("   2. Address FIFO Data Count (hi)  : %lu\n", rd_reg_45th_hi);
    //         printf("      Address FIFO Data Count (lo)  : %lu\n", rd_reg_45th_lo);

    //         // Tx Tstamp
    //         rd_reg_64th_hi = get_register(REG_64TH_HIGH);   // Tx Tstamp 1 (Upper)
    //         rd_reg_64th_lo = get_register(REG_64TH_LOW);    // Tx Tstamp 1 (Lower)
    //         rd_reg_65th_hi = get_register(REG_65TH_HIGH);   // Tx Tstamp 2 (Upper)
    //         rd_reg_65th_lo = get_register(REG_65TH_LOW);    // Tx Tstamp 2 (Lower)
    //         rd_reg_66th_hi = get_register(REG_66TH_HIGH);   // Tx Tstamp 3 (Upper)
    //         rd_reg_66th_lo = get_register(REG_66TH_LOW);    // Tx Tstamp 3 (Lower)
    //         rd_reg_67th_hi = get_register(REG_67TH_HIGH);   // Tx Tstamp 4 (Upper)
    //         rd_reg_67th_lo = get_register(REG_67TH_LOW);    // Tx Tstamp 4 (Lower)

    //         printf("   3. Tx Tstamp 1 (hi)     : %lu\n", rd_reg_64th_hi);
    //         printf("      Tx Tstamp 1 (lo)     : %lu\n", rd_reg_64th_lo);
    //         printf("      Tx Tstamp 2 (hi)     : %lu\n", rd_reg_65th_hi);
    //         printf("      Tx Tstamp 2 (lo)     : %lu\n", rd_reg_65th_lo);
    //         printf("      Tx Tstamp 3 (hi)     : %lu\n", rd_reg_66th_hi);
    //         printf("      Tx Tstamp 3 (lo)     : %lu\n", rd_reg_66th_lo);
    //         printf("      Tx Tstamp 4 (hi)     : %lu\n", rd_reg_67th_hi);
    //         printf("      Tx Tstamp 4 (lo)     : %lu\n", rd_reg_67th_lo);
    //         printf("\n");

    //         // FS Total Rx Frame Count
    //         rd_reg_24th_hi = get_register(REG_24TH_HIGH);   // FS Total Rx Frame Count (Upper)
    //         rd_reg_24th_lo = get_register(REG_24TH_LOW);    // FS Total Rx Frame Count (Lower)

    //         printf("   4. FS Total Rx Frame Count (hi)     : %lu\n", rd_reg_24th_hi);
    //         printf("      FS Total Rx Frame Count (lo)     : %lu\n", rd_reg_24th_lo);
    //         printf("\n");

    //         // FSCH Total New/Valid/Delay/Drop Entry Count
    //         rd_reg_37th_hi = get_register(REG_37TH_HIGH);   // FSCH Total New Entry Count   (Upper)
    //         rd_reg_37th_lo = get_register(REG_37TH_LOW);    // FSCH Total New Entry Count   (Lower)
    //         rd_reg_38th_hi = get_register(REG_38TH_HIGH);   // FSCH Total Valid Entry Count (Upper)
    //         rd_reg_38th_lo = get_register(REG_38TH_LOW);    // FSCH Total Valid Entry Count (Lower)
    //         rd_reg_39th_hi = get_register(REG_39TH_HIGH);   // FSCH Total Delay Entry Count (Upper)
    //         rd_reg_39th_lo = get_register(REG_39TH_LOW);    // FSCH Total Delay Entry Count (Lower)
    //         rd_reg_40th_hi = get_register(REG_40TH_HIGH);   // FSCH Total Drop Entry Count  (Upper)
    //         rd_reg_40th_lo = get_register(REG_40TH_LOW);    // FSCH Total Drop Entry Count  (Lower)

    //         printf("   5. FSCH Total New Entry Count   (hi)  : %lu\n", rd_reg_37th_hi);
    //         printf("      FSCH Total New Entry Count   (lo)  : %lu\n", rd_reg_37th_lo);
    //         printf("      FSCH Total Valid Entry Count (hi)  : %lu\n", rd_reg_38th_hi);
    //         printf("      FSCH Total Valid Entry Count (lo)  : %lu\n", rd_reg_38th_lo);
    //         printf("      FSCH Total Delay Entry Count (hi)  : %lu\n", rd_reg_39th_hi);
    //         printf("      FSCH Total Delay Entry Count (lo)  : %lu\n", rd_reg_39th_lo);
    //         printf("      FSCH Total Drop Entry Count  (hi)  : %lu\n", rd_reg_40th_hi);
    //         printf("      FSCH Total Drop Entry Count  (lo)  : %lu\n", rd_reg_40th_lo);
    //         printf("\n");

    //         // FT Total Tx Frame/Byte Count
    //         rd_reg_58th_hi = get_register(REG_58TH_HIGH);   // FT Total Tx Frame Count   (Upper)
    //         rd_reg_58th_lo = get_register(REG_58TH_LOW);    // FT Total Tx Frame Count   (Lower)
    //         rd_reg_59th_hi = get_register(REG_59TH_HIGH);   // FT Total Tx Frame Count   (Upper)
    //         rd_reg_59th_lo = get_register(REG_59TH_LOW);    // FT Total Tx Frame Count   (Lower)

    //         printf("   6. FT Total Tx Frame Count   (hi)  : %lu\n", rd_reg_58th_hi);
    //         printf("      FT Total Tx Frame Count   (lo)  : %lu\n", rd_reg_58th_lo);
    //         printf("      FT Total Tx Byte Count    (hi)  : %lu\n", rd_reg_59th_hi);
    //         printf("      FT Total Tx Byte Count    (lo)  : %lu\n", rd_reg_59th_lo);
    //         printf("\n");

    //     // ==================================================================== //
    //     //  [3] TSN Rx Information                                              //
    //     // ==================================================================== //
    //         printf("// TSN Rx Information\n");

    //         // Rx Tstamp
    //         rd_reg_1th_hi = get_register(REG_1TH_HIGH);   // Rx Tstamp (Upper)
    //         rd_reg_1th_lo = get_register(REG_1TH_LOW);    // Rx Tstamp (Lower)

    //         printf("   1. Rx Tstamp (hi)     : %lu\n", rd_reg_1th_hi);
    //         printf("      Rx Tstamp (lo)     : %lu\n", rd_reg_1th_lo);
    //         printf("\n");

    //         // FD Total Rx Frame Count
    //         rd_reg_4th_hi = get_register(REG_4TH_HIGH);   // Rx Tstamp (Upper)
    //         rd_reg_4th_lo = get_register(REG_4TH_LOW);    // Rx Tstamp (Lower)

    //         printf("   2. FD Total Rx Frame Count (hi) : %lu\n", rd_reg_4th_hi);
    //         printf("      FD Total Rx Frame Count (lo) : %lu\n", rd_reg_4th_lo);
    //         printf("\n");
            
    //         printf("\n\n\n\n");
            
    //         usleep(100*1000);

    //     }
        

    //     return 0;
    // }


    int sel_tx_path_1_test_app(void)
    {
        uint32_t temp_reg;

        temp_reg = get_register(REG_1TH_LOW);
        usleep(1*1000);

        set_register(REG_1TH_LOW, (temp_reg & ~(1U << 2)));
        printf("Tx Path 1 is Selected!\n\n");

        usleep(10*1000);

        return 0;
    }

    int sel_tx_path_2_test_app(void)
    {
        uint32_t temp_reg;

        temp_reg = get_register(REG_1TH_LOW);
        usleep(1*1000);

        set_register(REG_1TH_LOW, (temp_reg | (1U << 2)));
        printf("Tx Path 2 is Selected!\n\n");

        usleep(10*1000);

        return 0;
    }

    int sel_rx_path_1_test_app(void)
    {
        uint32_t temp_reg;

        temp_reg = get_register(REG_1TH_LOW);
        usleep(1*1000);

        set_register(REG_1TH_LOW, (temp_reg & ~(1U << 1)));
        printf("Rx Path 1 is Selected!\n\n");

        usleep(10*1000);

        return 0;
    }

    int sel_rx_path_2_test_app(void)
    {
        uint32_t temp_reg;

        temp_reg = get_register(REG_1TH_LOW);
        usleep(1*1000);

        set_register(REG_1TH_LOW, (temp_reg | (1U << 1)));
        printf("Rx Path 2 is Selected!\n\n");

        usleep(10*1000);

        return 0;
    }

/* ================================================================================================================================================ */

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

