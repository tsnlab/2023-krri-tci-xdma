#ifndef    __API_FTD2XX_H__
#define    __API_FTD2XX_H__

#define LOOPBACK_DATA_FILE_NAME \
        "./input/00.boardInput/input-node0003-trace010000-float4B.bin"

#define MLP_NODE_IN_FILE_NAME \
        "./input/00.boardInput/input-node0003-trace010000-float4B.bin"
#define MLP_OUT_FILE_PATH "./output/01.mlp_result_data/20221115/"
#define MLP_DEF_NODE_NUM  (3)
#define MLP_DEF_TRACE_NUM (10000)

#define FTD2XX_DEF_PORT_NUM  (0)
#define FTD2XX_DEF_DATA_SIZE (240)
#define FTD2XX_DEF_BAUD_RATE (115200)

int process_main_boardCmd(int argc, const char *argv[], 
                          menu_command_t *menu_tbl);
int process_main_configCmd(int argc, const char *argv[], 
                           menu_command_t *menu_tbl);
#endif    // __API_FTD2XX_H__
