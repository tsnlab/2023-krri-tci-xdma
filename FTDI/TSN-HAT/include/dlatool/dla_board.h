#ifndef    __DLA_BOARD_H__
#define    __DLA_BOARD_H__


/***             Control FPGA Register           ***/
/*
               Block                            |       Address    
  --------------------------------+----------------
  CFRB(Control FPGA Reg Block)    |    0x000 ~ 0x0FF    
  HUIB(Host USB IF Block)         |    0x100 ~ 0x1FF    
  DAB(Data Analysis Block)        |    0x200 ~ 0x2FF    
  MFDB (Main FPGA Data path Block)|    0x300 ~ 0x3FF    
  SMCB(Select Map Control Block)  |    0x400 ~ 0x4FF    

  0x000    Control_FPGA_Version_Number  R
  0x001    Control_FPGA_Control1        R/W
  0x002    Control_FPGA_Control2        R/W
  0x003    Scratch                      R/W
  0x010    LED_Control                  R/W
  0x011    Header_Status1               R
  0x012    Header_Status2               R
  0x013    Switch_Status                R
  0x020    CFR_FIFO_Status1             R
  0x021    CFR_FIFO_Status2             R
  0x022    CFT_FIFO_Status1             R
  0x023    CFT_FIFO_Status2             R
  0x100    HUIB_Control                 R/W
  0x101    HUIB_Status                  R
  0x110    HUR_FIFO_Status              R
  0x111    HUT_FIFO_Status              R
  0x200    DAB_Control                  R/W
  0x201    DAB_Status                   R
  0x300    MFDB_Control                 R/W
  0x301    MFDB_Status                  R
  0x310    MFR_FIFO_Status1             R
  0x311    MFR_FIFO_Status2             R
  0x312    MFT_FIFO_Status1             R
  0x313    MFT_FIFO_Status2             R
  0x400    SMCB_Control                 R/W
  0x401    SMCB_Status                  R
  0x410    SMR_FIFO_Status1             R
  0x411    SMR_FIFO_Status2             R

*/

/*** CFRB(Control FPGA Reg Block) Register Group ***/
#define  CONTROL_FPGA_VERSION_NUMBER  0x000        //    R
#define  CONTROL_FPGA_CONTROL1        0x001        //    R/W
#define  CONTROL_FPGA_CONTROL2        0x002        //    R/W
#define  SCRATCH                      0x003        //    R/W
#define  LED_CONTROL                  0x010        //    R/W
#define  HEADER_STATUS1               0x011        //    R
#define  HEADER_STATUS2               0x012        //    R
#define  SWITCH_STATUS                0x013        //    R
#define  CFR_FIFO_STATUS1             0x020        //    R
#define  CFR_FIFO_STATUS2             0x021        //    R
#define  CFT_FIFO_STATUS1             0x022        //    R
#define  CFT_FIFO_STATUS2             0x023        //    R

/*** HUIB(Host USB IF Block) Register Group ***/
#define  HUIB_CONTROL                 0x100        //    R/W
#define  HUIB_STATUS                  0x101        //    R
#define  HUR_FIFO_STATUS              0x110        //    R
#define  HUT_FIFO_STATUS              0x111        //    R

/*** DAB(Data Analysis Block) Register Group ***/
#define  DAB_CONTROL                  0x200        //    R/W
#define  DAB_STATUS                   0x201        //    R

/*** MFDB(Main FPGA Data path Block) Register Group ***/
#define  MFDB_CONTROL                 0x300        //    R/W
#define  MFDB_STATUS                  0x301        //    R
#define  MFR_FIFO_STATUS1             0x310        //    R
#define  MFR_FIFO_STATUS2             0x311        //    R
#define  MFT_FIFO_STATUS1             0x312        //    R
#define  MFT_FIFO_STATUS2             0x313        //    R

/*** SMCB(Select Map Control Block) Register Group ***/
#define  SMCB_CONTROL                 0x400        //    R/W
#define  SMCB_STATUS                  0x401        //    R
#define  SMCB_PACKET_COUNT            0x404        //    R
#define  SMR_FIFO_STATUS1             0x410        //    R
#define  SMR_FIFO_STATUS2             0x411        //    R


/***             Main FPGA Register           ***/
/*
                     Block                          |       Address    
    --------------------------------------+--------------
    MFRB(Main FPGA Reg Block)                            |    0x000 ~ 0x0FF    
    DPIB(Data Path IF Block)                            |    0x100 ~ 0x1FF    
    MIB(Memory IF Block)                                     |    0x200 ~ 0x2FF    
    DLAB(Deep Learning Accelerator Block) |    0x300 ~ 0x3FF    

    0x000    Main_FPGA_Version_Number    R
    0x001    Main_FPGA_Control1    R/W
    0x002    Main_FPGA_Control2    R/W
    0x003    Scratch    R/W
    0x010    LED_Control    R/W
    0x011    Header_Status1    R
    0x012    Header_Status2    R
    0x013    Switch_Status    R
    0x020    MFR_FIFO_Status1    R
    0x021    MFR_FIFO_Status2    R
    0x022    MFT_FIFO_Status1    R
    0x023    MFT_FIFO_Status2    R
    0x100    DPIB_Control    R/W
    0x101    DPIB_Status    R
    0x110    DPR_FIFO_Status1    R
    0x111    DPR_FIFO_Status2    R
    0x112    DPT_FIFO_Status1    R
    0x113    DPT_FIFO_Status2    R
    0x200    MIB_Control    R/W
    0x201    MIB_Status    R
    0x210    MIR1_FIFO_Status1    R
    0x211    MIR1_FIFO_Status2    R
    0x212    MIT1_FIFO_Status1    R
    0x213    MIT1_FIFO_Status2    R
    0x220    MIR2_FIFO_Status1    R
    0x221    MIR2_FIFO_Status2    R
    0x222    MIT2_FIFO_Status1    R
    0x223    MIT2_FIFO_Status2    R
    0x300    DLAB_Control    R/W
    0x301    DLAB_Status    R

*/

/*** MFRB(Main FPGA Reg Block) Register Group ***/
#define  MAIN_FPGA_VERSION_NUMBER 0x000        //    R
#define  MAIN_FPGA_CONTROL1       0x001        //    R/W
#define  MAIN_FPGA_CONTROL2       0x002        //    R/W
#define  MAIN_SCRATCH             0x003        //    R/W
#define  MAIN_LED_CONTROL         0x010        //    R/W
#define  MAIN_HEADER_STATUS1      0x011        //    R
#define  MAIN_HEADER_STATUS2      0x012        //    R
#define  MAIN_SWITCH_STATUS       0x013        //    R
#define  MAIN_MFR_FIFO_STATUS1    0x020        //    R
#define  MAIN_MFR_FIFO_STATUS2    0x021        //    R
#define  MAIN_MFT_FIFO_STATUS1    0x022        //    R
#define  MAIN_MFT_FIFO_STATUS2    0x023        //    R

/*** DPIB(Data Path IF Block) Register Group ***/
#define  MAIN_DPIB_CONTROL        0x100        //    R/W
#define  MAIN_DPIB_STATUS         0x101        //    R
#define  MAIN_DPR_FIFO_STATUS1    0x110        //    R
#define  MAIN_DPR_FIFO_STATUS2    0x111        //    R
#define  MAIN_DPT_FIFO_STATUS1    0x112        //    R
#define  MAIN_DPT_FIFO_STATUS2    0x113        //    R

/*** MIB(Memory IF Block) Register Group ***/
#define  MAIN_MIB_CONTROL         0x200        //    R/W
#define  MAIN_MIB_STATUS          0x201        //    R
#define  MAIN_MIR1_FIFO_STATUS1   0x210        //    R
#define  MAIN_MIR1_FIFO_STATUS2   0x211        //    R
#define  MAIN_MIT1_FIFO_STATUS1   0x212        //    R
#define  MAIN_MIT1_FIFO_STATUS2   0x213        //    R
#define  MAIN_MIR2_FIFO_STATUS1   0x220        //    R
#define  MAIN_MIR2_FIFO_STATUS2   0x221        //    R
#define  MAIN_MIT2_FIFO_STATUS1   0x222        //    R
#define  MAIN_MIT2_FIFO_STATUS2   0x223        //    R

/*** DLAB(Deep Learning Accelerator Block) Register Group ***/
#define  DLAB_CONTROL             0x300        //    R/W
#define  DLAB_STATUS              0x301        //    R

/*******************************************
 *        Register 
 *******************************************/
enum reg_type { RO = 0x0, RW = 0x1 };
#define BOARD_DEF_OPERATION (0)    // 0: read, 1: write
#define BOARD_DEF_FPGA      (0)    // 0: read, 1: write

typedef struct register_info {
    unsigned short a;
    unsigned short d;
    char *name;
    enum reg_type rw;
} __attribute__((__packed__)) regInfo_t;

#define SOD_LOW_BYTE  (0xCA)    // Start Of Data Low Byte
#define SOD_HIGH_BYTE (0xBA)    // Start Of Data Low Byte
enum data_dest { 
    DST_CFRB = 0x0, 
    DST_MFDB = 0x1, 
    DST_SMCB = 0x2, 
    DST_MFRB = 0x8, 
    DST_MIB  = 0x9, 
    DST_DLA  = 0xA, 
    DST_DPIB = 0xB
};

enum operate { 
    OPR_RD = 0x0, 
    OPR_WR = 0x3 
};

typedef struct board_message_header {
    unsigned char sodL;
    unsigned char sodH;
    unsigned char dst:4,
                  d_lenL:4;
    unsigned char d_lenH;
} __attribute__((__packed__)) bdMsgHdr_t;

typedef struct cfrb_message {
    bdMsgHdr_t        hdr;
    unsigned char op:2,
                  rAddrL:6;
    unsigned char rAddrH;
    unsigned char rDataL;
    unsigned char rDataH;
} __attribute__((__packed__)) cfrbMsg_t, mfrbMsg_t;

/*  FT2232H
    Buffer Sizes 
            TX: 4096 bytes/channel
            RX: 4096 bytes/channel
            When tested, the buffer size is 65536 bytes.
*/
#define MAX_BD_MSG_LEN   4095    // d_len: 12bits
#define MAX_BD_DATA_LEN  (MAX_BD_MSG_LEN - (int)(sizeof(bdMsgHdr_t)))
#define MAX_MIB_DATA_LEN (MAX_BD_DATA_LEN - 5)

//#define MAX_SCMB_DATAWORD_LEN (MAX_BD_DATA_LEN/(int)(sizeof(unsigned int)))    // 1022
//#define MAX_SCMB_DATAWORD_LEN 1000
#define MAX_SCMB_DATAWORD_LEN 950

typedef struct smcb_message {
    bdMsgHdr_t        hdr;
    unsigned int     data[MAX_SCMB_DATAWORD_LEN];
//    unsigned char data[MAX_BD_DATA_LEN];
} __attribute__((__packed__)) smcbMsg_t;

typedef struct mib_message {
    bdMsgHdr_t    hdr;
    unsigned char op:2,
                  rsv:6;
    unsigned char mAddrL;
    unsigned char mAddrM;
    unsigned char mAddrH;
    unsigned char mAddrT;
    unsigned char data[MAX_MIB_DATA_LEN];
} __attribute__((__packed__)) mibMsg_t;

#define MAX_DLAB_TX_MSG_LEN 120
typedef struct dlab_tx_message {
    bdMsgHdr_t    hdr;
    unsigned char data[MAX_DLAB_TX_MSG_LEN];
} __attribute__((__packed__)) dlabTxMsg_t;

#define MAX_DLAB_RX_MSG_LEN 16    
typedef struct dlab_rx_message {
    bdMsgHdr_t    hdr;
    unsigned char data[MAX_DLAB_RX_MSG_LEN];
} __attribute__((__packed__)) dlabRxMsg_t;

#define DLAB_AES_KEY_VALUE_LEN  16
#define DLAB_AES_PLAIN_TEXT_LEN 16
typedef struct aes_message {
    bdMsgHdr_t    hdr;
    unsigned char acL;
    unsigned char acH;
    unsigned char key[DLAB_AES_KEY_VALUE_LEN];
    unsigned char plain_text[DLAB_AES_PLAIN_TEXT_LEN];
} __attribute__((__packed__)) aesMsg_t;

#define MAX_MLP_NODE_CNT (1000)
#define MLP_RESULT_LEN   (16)
typedef struct mlp_tx_message {
    bdMsgHdr_t    hdr;
    float         plain_text[MAX_MLP_NODE_CNT];
} __attribute__((__packed__)) mlpTxMsg_t;

typedef struct mlp_rx_message {
    bdMsgHdr_t     hdr;
    unsigned char  result[MLP_RESULT_LEN];
} __attribute__((__packed__)) mlpRxMsg_t;

#endif    // __DLA_BOARD_H__
