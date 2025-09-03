/*
 * platform_config.h
 *
 *  Created on: Apr 22, 2023
 *      Author: pooky
 */

#ifndef PLATFORM_CONFIG_H_
#define PLATFORM_CONFIG_H_

#include <stdio.h>
#include "stdint-gcc.h"

// Correction values for TX, RX.
// Temac + PHY
#define TX_ADJUST_NS (708 + 60)
#define RX_ADJUST_NS (1920 + 240)

typedef uint64_t sysclock_t;
typedef uint64_t timestamp_t;

typedef uintptr_t UINTPTR;

#define PLATFORM_DEBUG

#define ONE_QUEUE_TSN

#define SDK_VERSION                                 (0x2309180D)
/*
 *     0x23050309 : TSN v1 0.7. First Release
 *     0x2305110A : Add RTT(Round Trip Time) test function
 *                  Add src/timer directory, timer_handler.c  timer_handler.h files
 *                  Add 1 second interval interrupt handler
 *     0x2305170B : Added an example function(int32_t transmit_arp_paket()) to send
 *                  a packet to the src/tsn/packet_handler.c file
 *     0x2305260C : Added P2P protocol function 
 *     0x2309180D : PCIe version
 */

#define REG_TSN_VERSION                             0x0000
#define REG_TSN_CONFIG                              0x0004
#define REG_TSN_CONTROL                             0x0008
#define REG_SCRATCH                                 0x0010

#define REG_QBV_SLOT_STATUS                         0x0028
#define REG_PULSE_AT_MSB                            0x002c
#define REG_PULSE_AT_LSB                            0x0030
#define REG_CYCLE_1S                                0x0034

#define REG_RX_PACKETS                              0x0100
#define REG_RX_BYTES_HIGH                           0x0110
#define REG_RX_BYTES_LOW                            0x0114
#define REG_RX_DROP_PACKETS                         0x0120
#define REG_RX_DROP_BYTES_HIGH                      0x0130
#define REG_RX_DROP_BYTES_LOW                       0x0134

#define REG_TX_PACKETS                              0x0200
#define REG_TX_BYTES_HIGH                           0x0210
#define REG_TX_BYTES_LOW                            0x0214
#define REG_TX_DROP_PACKETS                         0x0220
#define REG_TX_DROP_BYTES_HIGH                      0x0230
#define REG_TX_DROP_BYTES_LOW                       0x0234

#ifdef ONE_QUEUE_TSN
#define REG_TX_FAIL_PACKETS                         0x0240
#define REG_TX_FAIL_BYTES_MSB                       0x0250
#define REG_TX_FAIL_BYTES_LSB                       0x0254
#define REG_TX_DELAY_PACKETS                        0x0260
#define REG_TX_DELAY_BYTES_MSB                      0x0270
#define REG_TX_DELAY_BYTES_LSB                      0x0274
#endif

#define REG_TX_TIMESTAMP_COUNT                      0x0300
#define REG_TX_TIMESTAMP1_HIGH                      0x0310
#define REG_TX_TIMESTAMP1_LOW                       0x0314
#define REG_TX_TIMESTAMP2_HIGH                      0x0320
#define REG_TX_TIMESTAMP2_LOW                       0x0324
#define REG_TX_TIMESTAMP3_HIGH                      0x0330
#define REG_TX_TIMESTAMP3_LOW                       0x0334
#define REG_TX_TIMESTAMP4_HIGH                      0x0340
#define REG_TX_TIMESTAMP4_LOW                       0x0344
#define REG_SYS_COUNT_HIGH                          0x0380
#define REG_SYS_COUNT_LOW                           0x0384

#define REG_RX_INPUT_PACKET_COUNT                   0x0400
#define REG_RX_OUTPUT_PACKET_COUNT                  0x0404
#define REG_RX_BUFFER_FULL_DROP_PACKET_COUNT        0x0408
#define REG_TX_INPUT_PACKET_COUNT                   0x0410
#define REG_TX_OUTPUT_PACKET_COUNT                  0x0414
#define REG_TX_BUFFER_FULL_DROP_PACKET_COUNT        0x0418
#ifdef ONE_QUEUE_TSN
#define REG_NORMAL_TIMEOUT_COUNT                    0x041c
#define REG_TO_OVERFLOW_POPPED_COUNT                0x0420
#define REG_TO_OVERFLOW_TIMEOUT_COUNT               0x0424
#define REG_TIMEOUT_DROP_FROM                       0x0428
#define REG_TIMEOUT_DROP_TO                         0x042c
#define REG_TIMEOUT_DROP_SYS                        0x0434
#endif


#define REG_RPPB_FIFO_STATUS                        0x0470
#define REG_RASB_FIFO_STATUS                        0x0474

#ifdef ONE_QUEUE_TSN
#define REG_TX_AXIS_FIFO_STATUS1                    0x0480
#define REG_TX_AXIS_FIFO_STATUS                     0x0484
#define REG_TX_AXIS_BUFFER_STATUS                   0x0488
#define REG_TX_BACK_PRESSURE_EVENT_COUNT            0x048c
#define REG_RX_DEBUG                                0x04A0
#define REG_TX_DEBUG                                0x04B0
#else
#define REG_TASB_FIFO_STATUS                        0x0480
#define REG_TPPB_FIFO_STATUS                        0x0484
#define REG_MRIB_DEBUG                              0x04A0
#define REG_MTIB_DEBUG                              0x04B0
#endif

#define REG_TEMAC_STATUS                            0x0500
#define REG_TEMAC_RX_STAT                           0x0510
#define REG_TEMAC_TX_STAT                           0x0514


// User Register Definition
#define   REG_1TH_HIGH                  (0x1000 + 4*0)
#define   REG_1TH_LOW                   (0x1000 + 4*1)
#define   REG_2TH_HIGH                  (0x1000 + 4*2)
#define   REG_2TH_LOW                   (0x1000 + 4*3)
#define   REG_3TH_HIGH                  (0x1000 + 4*4)
#define   REG_3TH_LOW                   (0x1000 + 4*5)
#define   REG_4TH_HIGH                  (0x1000 + 4*6)
#define   REG_4TH_LOW                   (0x1000 + 4*7)
#define   REG_5TH_HIGH                  (0x1000 + 4*8)
#define   REG_5TH_LOW                   (0x1000 + 4*9)
#define   REG_6TH_HIGH                  (0x1000 + 4*10)
#define   REG_6TH_LOW                   (0x1000 + 4*11)
#define   REG_7TH_HIGH                  (0x1000 + 4*12)
#define   REG_7TH_LOW                   (0x1000 + 4*13)
#define   REG_8TH_HIGH                  (0x1000 + 4*14)
#define   REG_8TH_LOW                   (0x1000 + 4*15)
#define   REG_9TH_HIGH                  (0x1000 + 4*16)
#define   REG_9TH_LOW                   (0x1000 + 4*17)
#define   REG_10TH_HIGH                 (0x1000 + 4*18)
#define   REG_10TH_LOW                  (0x1000 + 4*19)
#define   REG_11TH_HIGH                 (0x1000 + 4*20)
#define   REG_11TH_LOW                  (0x1000 + 4*21)
#define   REG_12TH_HIGH                 (0x1000 + 4*22)
#define   REG_12TH_LOW                  (0x1000 + 4*23)
#define   REG_13TH_HIGH                 (0x1000 + 4*24)
#define   REG_13TH_LOW                  (0x1000 + 4*25)
#define   REG_14TH_HIGH                 (0x1000 + 4*26)
#define   REG_14TH_LOW                  (0x1000 + 4*27)
#define   REG_15TH_HIGH                 (0x1000 + 4*28)
#define   REG_15TH_LOW                  (0x1000 + 4*29)
#define   REG_16TH_HIGH                 (0x1000 + 4*30)
#define   REG_16TH_LOW                  (0x1000 + 4*31)
#define   REG_17TH_HIGH                 (0x1000 + 4*32)
#define   REG_17TH_LOW                  (0x1000 + 4*33)
#define   REG_18TH_HIGH                 (0x1000 + 4*34)
#define   REG_18TH_LOW                  (0x1000 + 4*35)
#define   REG_19TH_HIGH                 (0x1000 + 4*36)
#define   REG_19TH_LOW                  (0x1000 + 4*37)
#define   REG_20TH_HIGH                 (0x1000 + 4*38)
#define   REG_20TH_LOW                  (0x1000 + 4*39)
#define   REG_21TH_HIGH                 (0x1000 + 4*40)
#define   REG_21TH_LOW                  (0x1000 + 4*41)
#define   REG_22TH_HIGH                 (0x1000 + 4*42)
#define   REG_22TH_LOW                  (0x1000 + 4*43)
#define   REG_23TH_HIGH                 (0x1000 + 4*44)
#define   REG_23TH_LOW                  (0x1000 + 4*45)
#define   REG_24TH_HIGH                 (0x1000 + 4*46)
#define   REG_24TH_LOW                  (0x1000 + 4*47)
#define   REG_25TH_HIGH                 (0x1000 + 4*48)
#define   REG_25TH_LOW                  (0x1000 + 4*49)
#define   REG_26TH_HIGH                 (0x1000 + 4*50)
#define   REG_26TH_LOW                  (0x1000 + 4*51)
#define   REG_27TH_HIGH                 (0x1000 + 4*52)
#define   REG_27TH_LOW                  (0x1000 + 4*53)
#define   REG_28TH_HIGH                 (0x1000 + 4*54)
#define   REG_28TH_LOW                  (0x1000 + 4*55)
#define   REG_29TH_HIGH                 (0x1000 + 4*56)
#define   REG_29TH_LOW                  (0x1000 + 4*57)
#define   REG_30TH_HIGH                 (0x1000 + 4*58)
#define   REG_30TH_LOW                  (0x1000 + 4*59)
#define   REG_31TH_HIGH                 (0x1000 + 4*60)
#define   REG_31TH_LOW                  (0x1000 + 4*61)
#define   REG_32TH_HIGH                 (0x1000 + 4*62)
#define   REG_32TH_LOW                  (0x1000 + 4*63)
#define   REG_33TH_HIGH                 (0x1000 + 4*64)
#define   REG_33TH_LOW                  (0x1000 + 4*65)
#define   REG_34TH_HIGH                 (0x1000 + 4*66)
#define   REG_34TH_LOW                  (0x1000 + 4*67)
#define   REG_35TH_HIGH                 (0x1000 + 4*68)
#define   REG_35TH_LOW                  (0x1000 + 4*69)
#define   REG_36TH_HIGH                 (0x1000 + 4*70)
#define   REG_36TH_LOW                  (0x1000 + 4*71)
#define   REG_37TH_HIGH                 (0x1000 + 4*72)
#define   REG_37TH_LOW                  (0x1000 + 4*73)
#define   REG_38TH_HIGH                 (0x1000 + 4*74)
#define   REG_38TH_LOW                  (0x1000 + 4*75)
#define   REG_39TH_HIGH                 (0x1000 + 4*76)
#define   REG_39TH_LOW                  (0x1000 + 4*77)
#define   REG_40TH_HIGH                 (0x1000 + 4*78)
#define   REG_40TH_LOW                  (0x1000 + 4*79)
#define   REG_41TH_HIGH                 (0x1000 + 4*80)
#define   REG_41TH_LOW                  (0x1000 + 4*81)
#define   REG_42TH_HIGH                 (0x1000 + 4*82)
#define   REG_42TH_LOW                  (0x1000 + 4*83)
#define   REG_43TH_HIGH                 (0x1000 + 4*84)
#define   REG_43TH_LOW                  (0x1000 + 4*85)
#define   REG_44TH_HIGH                 (0x1000 + 4*86)
#define   REG_44TH_LOW                  (0x1000 + 4*87)
#define   REG_45TH_HIGH                 (0x1000 + 4*88)
#define   REG_45TH_LOW                  (0x1000 + 4*89)
#define   REG_46TH_HIGH                 (0x1000 + 4*90)
#define   REG_46TH_LOW                  (0x1000 + 4*91)
#define   REG_47TH_HIGH                 (0x1000 + 4*92)
#define   REG_47TH_LOW                  (0x1000 + 4*93)
#define   REG_48TH_HIGH                 (0x1000 + 4*94)
#define   REG_48TH_LOW                  (0x1000 + 4*95)
#define   REG_49TH_HIGH                 (0x1000 + 4*96)
#define   REG_49TH_LOW                  (0x1000 + 4*97)
#define   REG_50TH_HIGH                 (0x1000 + 4*98)
#define   REG_50TH_LOW                  (0x1000 + 4*99)
#define   REG_51TH_HIGH                 (0x1000 + 4*100)
#define   REG_51TH_LOW                  (0x1000 + 4*101)
#define   REG_52TH_HIGH                 (0x1000 + 4*102)
#define   REG_52TH_LOW                  (0x1000 + 4*103)
#define   REG_53TH_HIGH                 (0x1000 + 4*104)
#define   REG_53TH_LOW                  (0x1000 + 4*105)
#define   REG_54TH_HIGH                 (0x1000 + 4*106)
#define   REG_54TH_LOW                  (0x1000 + 4*107)
#define   REG_55TH_HIGH                 (0x1000 + 4*108)
#define   REG_55TH_LOW                  (0x1000 + 4*109)
#define   REG_56TH_HIGH                 (0x1000 + 4*110)
#define   REG_56TH_LOW                  (0x1000 + 4*111)
#define   REG_57TH_HIGH                 (0x1000 + 4*112)
#define   REG_57TH_LOW                  (0x1000 + 4*113)
#define   REG_58TH_HIGH                 (0x1000 + 4*114)
#define   REG_58TH_LOW                  (0x1000 + 4*115)
#define   REG_59TH_HIGH                 (0x1000 + 4*116)
#define   REG_59TH_LOW                  (0x1000 + 4*117)
#define   REG_60TH_HIGH                 (0x1000 + 4*118)
#define   REG_60TH_LOW                  (0x1000 + 4*119)
#define   REG_61TH_HIGH                 (0x1000 + 4*120)
#define   REG_61TH_LOW                  (0x1000 + 4*121)
#define   REG_62TH_HIGH                 (0x1000 + 4*122)
#define   REG_62TH_LOW                  (0x1000 + 4*123)
#define   REG_63TH_HIGH                 (0x1000 + 4*124)
#define   REG_63TH_LOW                  (0x1000 + 4*125)
#define   REG_64TH_HIGH                 (0x1000 + 4*126)
#define   REG_64TH_LOW                  (0x1000 + 4*127)
#define   REG_65TH_HIGH                 (0x1000 + 4*128)
#define   REG_65TH_LOW                  (0x1000 + 4*129)
#define   REG_66TH_HIGH                 (0x1000 + 4*130)
#define   REG_66TH_LOW                  (0x1000 + 4*131)
#define   REG_67TH_HIGH                 (0x1000 + 4*132)
#define   REG_67TH_LOW                  (0x1000 + 4*133)
#define   REG_68TH_HIGH                 (0x1000 + 4*134)
#define   REG_68TH_LOW                  (0x1000 + 4*135)
#define   REG_69TH_HIGH                 (0x1000 + 4*136)
#define   REG_69TH_LOW                  (0x1000 + 4*137)
#define   REG_70TH_HIGH                 (0x1000 + 4*138)
#define   REG_70TH_LOW                  (0x1000 + 4*139)
#define   REG_71TH_HIGH                 (0x1000 + 4*140)
#define   REG_71TH_LOW                  (0x1000 + 4*141)
#define   REG_72TH_HIGH                 (0x1000 + 4*142)
#define   REG_72TH_LOW                  (0x1000 + 4*143)
#define   REG_73TH_HIGH                 (0x1000 + 4*144)
#define   REG_73TH_LOW                  (0x1000 + 4*145)
#define   REG_74TH_HIGH                 (0x1000 + 4*146)
#define   REG_74TH_LOW                  (0x1000 + 4*147)
#define   REG_75TH_HIGH                 (0x1000 + 4*148)
#define   REG_75TH_LOW                  (0x1000 + 4*149)
#define   REG_76TH_HIGH                 (0x1000 + 4*150)
#define   REG_76TH_LOW                  (0x1000 + 4*151)
#define   REG_77TH_HIGH                 (0x1000 + 4*152)
#define   REG_77TH_LOW                  (0x1000 + 4*153)
#define   REG_78TH_HIGH                 (0x1000 + 4*154)
#define   REG_78TH_LOW                  (0x1000 + 4*155)
#define   REG_79TH_HIGH                 (0x1000 + 4*156)
#define   REG_79TH_LOW                  (0x1000 + 4*157)
#define   REG_80TH_HIGH                 (0x1000 + 4*158)
#define   REG_80TH_LOW                  (0x1000 + 4*159)
#define   REG_81TH_HIGH                 (0x1000 + 4*160)
#define   REG_81TH_LOW                  (0x1000 + 4*161)
#define   REG_82TH_HIGH                 (0x1000 + 4*162)
#define   REG_82TH_LOW                  (0x1000 + 4*163)
#define   REG_83TH_HIGH                 (0x1000 + 4*164)
#define   REG_83TH_LOW                  (0x1000 + 4*165)
#define   REG_84TH_HIGH                 (0x1000 + 4*166)
#define   REG_84TH_LOW                  (0x1000 + 4*167)
#define   REG_85TH_HIGH                 (0x1000 + 4*168)
#define   REG_85TH_LOW                  (0x1000 + 4*169)
#define   REG_86TH_HIGH                 (0x1000 + 4*170)
#define   REG_86TH_LOW                  (0x1000 + 4*171)
#define   REG_87TH_HIGH                 (0x1000 + 4*172)
#define   REG_87TH_LOW                  (0x1000 + 4*173)
#define   REG_88TH_HIGH                 (0x1000 + 4*174)
#define   REG_88TH_LOW                  (0x1000 + 4*175)
#define   REG_89TH_HIGH                 (0x1000 + 4*176)
#define   REG_89TH_LOW                  (0x1000 + 4*177)
#define   REG_90TH_HIGH                 (0x1000 + 4*178)
#define   REG_90TH_LOW                  (0x1000 + 4*179)
#define   REG_91TH_HIGH                 (0x1000 + 4*180)
#define   REG_91TH_LOW                  (0x1000 + 4*181)
#define   REG_92TH_HIGH                 (0x1000 + 4*182)
#define   REG_92TH_LOW                  (0x1000 + 4*183)
#define   REG_93TH_HIGH                 (0x1000 + 4*184)
#define   REG_93TH_LOW                  (0x1000 + 4*185)
#define   REG_94TH_HIGH                 (0x1000 + 4*186)
#define   REG_94TH_LOW                  (0x1000 + 4*187)
#define   REG_95TH_HIGH                 (0x1000 + 4*188)
#define   REG_95TH_LOW                  (0x1000 + 4*189)
#define   REG_96TH_HIGH                 (0x1000 + 4*190)
#define   REG_96TH_LOW                  (0x1000 + 4*191)
#define   REG_97TH_HIGH                 (0x1000 + 4*192)
#define   REG_97TH_LOW                  (0x1000 + 4*193)
#define   REG_98TH_HIGH                 (0x1000 + 4*194)
#define   REG_98TH_LOW                  (0x1000 + 4*195)
#define   REG_99TH_HIGH                 (0x1000 + 4*196)
#define   REG_99TH_LOW                  (0x1000 + 4*197)
#define   REG_100TH_HIGH                (0x1000 + 4*198)
#define   REG_100TH_LOW                 (0x1000 + 4*199)
#define   REG_101TH_HIGH                (0x1000 + 4*200)
#define   REG_101TH_LOW                 (0x1000 + 4*201)
#define   REG_102TH_HIGH                (0x1000 + 4*202)
#define   REG_102TH_LOW                 (0x1000 + 4*203)
#define   REG_103TH_HIGH                (0x1000 + 4*204)
#define   REG_103TH_LOW                 (0x1000 + 4*205)
#define   REG_104TH_HIGH                (0x1000 + 4*206)
#define   REG_104TH_LOW                 (0x1000 + 4*207)
#define   REG_105TH_HIGH                (0x1000 + 4*208)
#define   REG_105TH_LOW                 (0x1000 + 4*209)
#define   REG_106TH_HIGH                (0x1000 + 4*210)
#define   REG_106TH_LOW                 (0x1000 + 4*211)
#define   REG_107TH_HIGH                (0x1000 + 4*212)
#define   REG_107TH_LOW                 (0x1000 + 4*213)
#define   REG_108TH_HIGH                (0x1000 + 4*214)
#define   REG_108TH_LOW                 (0x1000 + 4*215)
#define   REG_109TH_HIGH                (0x1000 + 4*216)
#define   REG_109TH_LOW                 (0x1000 + 4*217)
#define   REG_110TH_HIGH                (0x1000 + 4*218)
#define   REG_110TH_LOW                 (0x1000 + 4*219)






#ifdef ONE_QUEUE_TSN
#define REG_TEMAC_FCS_COUNT                         0x0520
#endif

#define TSCB_ADDRESS                            (0x44C00000)

#define DUMPREG_GENERAL 0x01
#define DUMPREG_RX      0x02
#define DUMPREG_TX      0x04
#define XDMA_REG_H2C    0x08
#define XDMA_REG_C2H    0x10
#define XDMA_REG_IRQ    0x20
#define XDMA_REG_CON    0x40
#define XDMA_REG_H2CS   0x80
#define XDMA_REG_C2HS   0x100
#define XDMA_REG_SCOM   0x200
#define XDMA_REG_MSIX   0x400
#define DUMPREG_ALL     0x7FF


#ifdef ONE_QUEUE_TSN
#define XDMA_SECTION_TAKEN_TICKS (500)
#define XDMA_SECTION_TICKS_MARGIN (18600)
#define DELAY_TICKS (50000)
#define DELAY_TICKS_MARGIN (5000)
#endif

struct rx_metadata {
    uint64_t timestamp;
#ifndef ONE_QUEUE_TSN
    union {
        uint16_t vlan_tag;
        struct {
            uint8_t vlan_prio :3;
            uint8_t vlan_cfi  :1;
            uint16_t vlan_vid :12;
        };
    };
    uint32_t checksum;
#endif
    uint16_t frame_length;
} __attribute__((packed, scalar_storage_order("big-endian")));

#ifdef ONE_QUEUE_TSN
struct tick_count {
    uint32_t tick:29;
    uint32_t priority:3;
} __attribute__((packed, scalar_storage_order("big-endian")));
#endif

struct tx_metadata {
#ifdef ONE_QUEUE_TSN
    struct tick_count from;
    struct tick_count to;
    struct tick_count delay_from;
    struct tick_count delay_to;
    uint16_t frame_length;
    uint16_t timestamp_id;
    uint8_t fail_policy;
    uint8_t reserved0[3];
    uint32_t reserved1;
    uint32_t reserved2;
#else
    union {
        uint16_t vlan_tag;
        struct {
            uint8_t vlan_prio :3;
            uint8_t vlan_cfi  :1;
            uint16_t vlan_vid :12;
        };
    };
    uint16_t timestamp_id;
    uint16_t frame_length;
    uint16_t reserved;
#endif
} __attribute__((packed, scalar_storage_order("big-endian")));

#define MAX_PACKET_LEN 1536

struct tsn_rx_buffer {
    struct rx_metadata metadata;
    uint8_t data[MAX_PACKET_LEN];
};

struct tsn_tx_buffer {
    struct tx_metadata metadata;
    uint8_t data[MAX_PACKET_LEN];
};

struct reginfo {
    char* name;
    int offset;
};


void dump_registers(int dumpflag, int on);

uint64_t get_sys_count();
uint64_t get_tx_timestamp(int timestamp_id);
uint64_t get_my_count();

uint32_t get_register(int offset);
int set_register(int offset, uint32_t val);

#endif /* PLATFORM_CONFIG_H_ */
