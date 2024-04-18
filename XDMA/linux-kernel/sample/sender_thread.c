/*
* TSNv1 XDMA :
* -------------------------------------------------------------------------------
# Copyrights (c) 2023 TSN Lab. All rights reserved.
# Programmed by hounjoung@tsnlab.com
#
# Revision history
# 2023-xx-xx    hounjoung   create this file.
# $Id$
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/time.h>


#include "error_define.h"
#include "receiver_thread.h"
#include "platform_config.h"

#include "ethernet.h"
#include "ip.h"
#include "ipv4.h"
#include "icmp.h"
#include "udp.h"
#include "arp.h"
#include "gptp.h"

#include "util.h"
#include "tsn.h"

#include "../libxdma/api_xdma.h"
#include "buffer_handler.h"

/******************** Constant Definitions **********************************/
#define HW_ADDR_LEN 6
#define IP_ADDR_LEN 4

#define FCS_LEN 4

#define RESERVED_TX_COUNT 4

#ifndef __RASPBERRY_PI_HAT_MODULE__
static const char* myMAC = "\x00\x11\x22\x33\x44\x55";
#endif

stats_t tx_stats[NUMBER_OF_ETH_PORT];

char tx_devname[NUMBER_OF_ETH_PORT][MAX_DEVICE_NAME];
int tx_fd[NUMBER_OF_ETH_PORT];

extern int tx_thread_run;

static int transmit_tsn_packet_no_free(struct tsn_tx_buffer* packet, int port) {

    uint64_t mem_len = sizeof(packet->metadata) + packet->metadata.frame_length;
    uint64_t bytes_tr;
    int status = 0;

    if (mem_len >= MAX_BUFFER_LENGTH) {
        printf("%s - %p length(%ld) is out of range(%d)\r\n", __func__, packet, mem_len, MAX_BUFFER_LENGTH);
        return XST_FAILURE;
    }

    status = xdma_api_write_from_buffer_with_fd(tx_devname[port], tx_fd[port], (char *)packet,
                                       mem_len, &bytes_tr);

    if(status != XST_SUCCESS) {
        tx_stats[port].txErrors++;
    } else {
        tx_stats[port].txPackets++;
        tx_stats[port].txBytes += bytes_tr;
    }

    return status;
}

static int process_send_packet(struct tsn_rx_buffer* rx, int port) {

#ifdef __RASPBERRY_PI_HAT_MODULE__
    uint8_t *buffer =(uint8_t *)rx;
    int tx_len;
    // Reuse RX buffer as TX
    struct tsn_tx_buffer* tx = (struct tsn_tx_buffer*)(buffer + sizeof(struct rx_metadata) - sizeof(struct tx_metadata));
    struct rx_metadata* rx_metadata = &rx->metadata;
    struct tx_metadata* tx_metadata = &tx->metadata;

    tx_len = rx_metadata->frame_length - (sizeof(struct rx_metadata) - sizeof(struct tx_metadata));

    // make tx metadata
    // tx_metadata->vlan_tag = rx->metadata.vlan_tag;
    tx_metadata->timestamp_id = 0;
    tx_metadata->reserved = 0;

    tx_metadata->frame_length = tx_len;
    transmit_tsn_packet_no_free(tx, port); 
    return XST_SUCCESS;
#else
        uint8_t *buffer =(uint8_t *)rx;
    int tx_len;
    // Reuse RX buffer as TX
    struct tsn_tx_buffer* tx = (struct tsn_tx_buffer*)(buffer + sizeof(struct rx_metadata) - sizeof(struct tx_metadata));
    struct tx_metadata* tx_metadata = &tx->metadata;
    uint8_t* rx_frame = (uint8_t*)&rx->data;
    uint8_t* tx_frame = (uint8_t*)&tx->data;
    struct ethernet_header* rx_eth = (struct ethernet_header*)rx_frame;
    struct ethernet_header* tx_eth = (struct ethernet_header*)tx_frame;

    // make tx metadata
    // tx_metadata->vlan_tag = rx->metadata.vlan_tag;
    tx_metadata->timestamp_id = 0;
    tx_metadata->reserved = 0;

    // make ethernet frame
    memcpy(&(tx_eth->dmac), &(rx_eth->smac), 6);
    memcpy(&(tx_eth->smac), myMAC, 6);
    // tx_eth->type = rx_eth->type;

    tx_len = ETH_HLEN;

    // do arp, udp echo, etc.
    switch (rx_eth->type) {
    case ETH_TYPE_ARP: // arp
        ;
        struct arp_header* rx_arp = (struct arp_header*)ETH_PAYLOAD(rx_frame);
        struct arp_header* tx_arp = (struct arp_header*)ETH_PAYLOAD(tx_frame);
        if (rx_arp->opcode != ARP_OPCODE_ARP_REQUEST) { return XST_FAILURE; }

        // make arp packet
        // tx_arp->hw_type = rx_arp->hw_type;
        // tx_arp->proto_type = rx_arp->proto_type;
        // tx_arp->hw_size = rx_arp->hw_size;
        // tx_arp->proto_size = rx_arp->proto_size;
        tx_arp->opcode = ARP_OPCODE_ARP_REPLY;
        memcpy(tx_arp->target_hw, rx_arp->sender_hw, HW_ADDR_LEN);
        memcpy(tx_arp->sender_hw, myMAC, HW_ADDR_LEN);
        uint8_t sender_proto[4];
        memcpy(sender_proto, rx_arp->sender_proto, IP_ADDR_LEN);
        memcpy(tx_arp->sender_proto, rx_arp->target_proto, IP_ADDR_LEN);
        memcpy(tx_arp->target_proto, sender_proto, IP_ADDR_LEN);

        tx_len += ARP_HLEN;
        break;
    case ETH_TYPE_IPv4: // ip
        ;
        struct ipv4_header* rx_ipv4 = (struct ipv4_header*)ETH_PAYLOAD(rx_frame);
        struct ipv4_header* tx_ipv4 = (struct ipv4_header*)ETH_PAYLOAD(tx_frame);

        uint32_t src;

        // Fill IPv4 header
        // memcpy(tx_ipv4, rx_ipv4, IPv4_HLEN(rx_ipv4));
        src = rx_ipv4->dst;
        tx_ipv4->dst = rx_ipv4->src;
        tx_ipv4->src = src;
        tx_len += IPv4_HLEN(rx_ipv4);

        if (rx_ipv4->proto == IP_PROTO_ICMP) {
            struct icmp_header* rx_icmp = (struct icmp_header*)IPv4_PAYLOAD(rx_ipv4);

            if (rx_icmp->type != ICMP_TYPE_ECHO_REQUEST) { return XST_FAILURE; }

            struct icmp_header* tx_icmp = (struct icmp_header*)IPv4_PAYLOAD(tx_ipv4);
            unsigned long icmp_len = IPv4_BODY_LEN(rx_ipv4);

            // Fill ICMP header and body
            // memcpy(tx_icmp, rx_icmp, icmp_len);
            tx_icmp->type = ICMP_TYPE_ECHO_REPLY;
            icmp_checksum(tx_icmp, icmp_len);
            tx_len += icmp_len;

        } else if (rx_ipv4->proto == IP_PROTO_UDP){
            struct udp_header* rx_udp = (struct udp_header*)IPv4_PAYLOAD(rx_ipv4);
            if (rx_udp->dstport != 7) { return XST_FAILURE; }

            struct udp_header* tx_udp = IPv4_PAYLOAD(tx_ipv4);

            // Fill UDP header
            // memcpy(tx_udp, rx_udp, rx_udp->length);
            uint16_t srcport;
            srcport = rx_udp->dstport;
            tx_udp->dstport = rx_udp->srcport;
            tx_udp->srcport = srcport;
            tx_udp->checksum = 0;
            tx_len += rx_udp->length; // UDP.length contains header length
        } else {
            return XST_FAILURE;
        }
        break;
    default:
        printf_debug("Unknown type: %04x\n", rx_eth->type);
        return XST_FAILURE;
    }
    tx_metadata->frame_length = tx_len;
    transmit_tsn_packet_no_free(tx, port);
    return XST_SUCCESS;
#endif
}

static void sender_in_normal_mode(char* devname, int fd, uint64_t size, int port) {

    QueueElement buffer = NULL;
    int status;

    while (tx_thread_run) {
        buffer = NULL;
        buffer = xbuffer_dequeue(port);
        if(buffer == NULL) {
            continue;
        }

        status = process_send_packet((struct tsn_rx_buffer*)buffer, port);
        if(status == XST_FAILURE) {
            tx_stats[port].txFiltered++;
        }

        buffer_pool_free(port, (BUF_POINTER)buffer);
    }
}

void* sender_thread(void* arg) {

    tx_thread_arg_t* p_arg = (tx_thread_arg_t*)arg;
    int port;

    printf(">>> %s(devname: %s, fn: %s, mode: %d, size: %d, port: %d)\n", 
               __func__, p_arg->devname, p_arg->fn, 
               p_arg->mode, p_arg->size, p_arg->port);

    port = p_arg->port;

    memset(&tx_devname[port][0], 0, MAX_DEVICE_NAME);
    memcpy(&tx_devname[port][0], p_arg->devname, MAX_DEVICE_NAME);

    if(xdma_api_dev_open(p_arg->devname, 0 /* eop_flush */, &tx_fd[port])) {
        printf("FAILURE: Could not open %s. Make sure xdma device driver is loaded and you have access rights (maybe use sudo?).\n", p_arg->devname);
        printf("<<< %s\n", __func__);
        return NULL;
    }

    initialize_statistics(&tx_stats[port]);

    switch(p_arg->mode) {
    case RUN_MODE_NORMAL:
        sender_in_normal_mode(p_arg->devname, tx_fd[port], p_arg->size, port);
    break;
    default:
        printf("%s - Unknown mode(%d)\n", __func__, p_arg->mode);
    break;
    }

    close(tx_fd[port]);
    printf("<<< %s()\n", __func__);

    return NULL;
}

