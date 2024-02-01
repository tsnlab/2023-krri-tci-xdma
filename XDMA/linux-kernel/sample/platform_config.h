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

//#define __USE_MULTI_CORE__

#define SDK_VERSION                                 (0x2305260C)
/*
 *     0x23050309 : TSN v1 0.7. First Release
 *     0x2305110A : Add RTT(Round Trip Time) test function
 *                  Add src/timer directory, timer_handler.c  timer_handler.h files
 *                  Add 1 second interval interrupt handler
 *     0x2305170B : Added an example function(int32_t transmit_arp_paket()) to send
 *                  a packet to the src/tsn/packet_handler.c file
 *     0x2305260C : Added P2P protocol function 
 */

#define REG_TSN_VERSION                             0x0000
#define REG_TSN_CONFIG                              0x0004
#define REG_TSN_CONTROL                             0x0008
#define REG_SCRATCH                                 0x0010
#define REG_ONE_SECOND_CNT                          0x0020 
#define REG_GPTP_SLAVE_PPS_OFFSET                   0x0024
#define REG_QBV_SLOT_STATUS                         0x0028

#define REG_TASB_MIN_STARTING_DATA_CNT              0x0610
#define REG_TPPB_MIN_STARTING_DATA_CNT              0x0620

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

#define REG_RPPB_FIFO_STATUS                        0x0470
#define REG_RASB_FIFO_STATUS                        0x0474
#define REG_TASB_FIFO_STATUS                        0x0480
#define REG_TPPB_FIFO_STATUS                        0x0484
#define REG_MRIB_DEBUG                              0x04A0
#define REG_MTIB_DEBUG                              0x04B0

#define REG_TEMAC_STATUS                            0x0500
#define REG_TEMAC_RX_STAT                           0x0510
#define REG_TEMAC_TX_STAT                           0x0514

#define TSCB_ADDRESS                            (0x44C00000)

#define H2C_CHANNEL_IDENTIFIER                      (0x0000)
#define H2C_CHANNEL_CONTROL1                        (0x0004)
#define H2C_CHANNEL_CONTROL2                        (0x0008)
#define H2C_CHANNEL_CONTROL3                        (0x000C)
#define H2C_CHANNEL_STATUS1                         (0x0040)
#define H2C_CHANNEL_STATUS2                         (0x0044)
#define H2C_CHANNEL_COMPLETED_DESCRIPTOR_COUNT      (0x0048)
#define H2C_CHANNEL_ALIGNMENTS                      (0x004C)
#define H2C_POLL_MODE_LOW_WRITE_BACK_ADDRESS        (0x0088)
#define H2C_POLL_MODE_HIGH_WRITE_BACK_ADDRESS       (0x008C)
#define H2C_CHANNEL_INTERRUPT_ENABLE_MASK1          (0x0090)
#define H2C_CHANNEL_INTERRUPT_ENABLE_MASK2          (0x0094)
#define H2C_CHANNEL_INTERRUPT_ENABLE_MASK3          (0x0098)
#define H2C_CHANNEL_PERFORMANCE_MONITOR_CONTROL     (0x00C0)
#define H2C_CHANNEL_PERFORMANCE_CYCLE_COUNT_L       (0x00C4)
#define H2C_CHANNEL_PERFORMANCE_CYCLE_COUNT_H       (0x00C8)
#define H2C_CHANNEL_PERFORMANCE_DATA_COUNT_L        (0x00CC)
#define H2C_CHANNEL_PERFORMANCE_DATA_COUNT_H        (0x00D0)

#define C2H_CHANNEL_IDENTIFIER                      (0x1000)
#define C2H_CHANNEL_CONTROL1                        (0x1004)
#define C2H_CHANNEL_CONTROL2                        (0x1008)
#define C2H_CHANNEL_CONTROL3                        (0x100C)
#define C2H_CHANNEL_STATUS1                         (0x1040)
#define C2H_CHANNEL_STATUS2                         (0x1044)
#define C2H_CHANNEL_COMPLETED_DESCRIPTOR_COUNT      (0x1048)
#define C2H_CHANNEL_ALIGNMENTS                      (0x104C)
#define C2H_POLL_MODE_LOW_WRITE_BACK_ADDRESS        (0x1088)
#define C2H_POLL_MODE_HIGH_WRITE_BACK_ADDRESS       (0x108C)
#define C2H_CHANNEL_INTERRUPT_ENABLE_MASK1          (0x1090)
#define C2H_CHANNEL_INTERRUPT_ENABLE_MASK2          (0x1094)
#define C2H_CHANNEL_INTERRUPT_ENABLE_MASK3          (0x1098)
#define C2H_CHANNEL_PERFORMANCE_MONITOR_CONTROL     (0x10C0)
#define C2H_CHANNEL_PERFORMANCE_CYCLE_COUNT_L       (0x10C4)
#define C2H_CHANNEL_PERFORMANCE_CYCLE_COUNT_H       (0x10C8)
#define C2H_CHANNEL_PERFORMANCE_DATA_COUNT_L        (0x10CC)
#define C2H_CHANNEL_PERFORMANCE_DATA_COUNT_H        (0x10D0)

#define DUMPREG_GENERAL 0x1
#define DUMPREG_RX      0x2
#define DUMPREG_TX      0x4
#define DUMPREG_H2C     0x8
#define DUMPREG_C2H     0x10
#define DUMPREG_ALL     0x1F

struct rx_metadata {
    uint64_t timestamp;
    union {
        uint16_t vlan_tag;
        struct {
            uint8_t vlan_prio :3;
            uint8_t vlan_cfi  :1;
            uint16_t vlan_vid :12;
        };
    };
    uint32_t checksum;
    uint16_t frame_length;
} __attribute__((packed, scalar_storage_order("big-endian")));

struct tx_metadata {
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

uint32_t get_register(int offset);
int set_register(int offset, uint32_t val);

#endif /* PLATFORM_CONFIG_H_ */
