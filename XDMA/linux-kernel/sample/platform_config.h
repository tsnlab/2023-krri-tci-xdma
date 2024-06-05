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

#define REG_TX_FAIL_PACKETS                         0x0240
#define REG_TX_FAIL_BYTES_MSB                       0x0250
#define REG_TX_FAIL_BYTES_LSB                       0x0254
#define REG_TX_DELAY_PACKETS                        0x0260
#define REG_TX_DELAY_BYTES_MSB                      0x0270
#define REG_TX_DELAY_BYTES_LSB                      0x0274

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
#define REG_TEMAC_FCS_COUNT                         0x0520

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

uint32_t get_register(int offset);
int set_register(int offset, uint32_t val);

#endif /* PLATFORM_CONFIG_H_ */
