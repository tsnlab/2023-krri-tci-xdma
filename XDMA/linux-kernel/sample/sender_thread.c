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

static const char* myMAC = "\x00\x11\x22\x33\x44\x55";

stats_t tx_stats;

char tx_devname[MAX_DEVICE_NAME];
int tx_fd;

extern int tx_thread_run;

#ifdef ONE_QUEUE_TSN
char* g_tx_buffer;
#endif

static struct tsn_tx_buffer* dequeue() {
    timestamp_t now = gptp_get_timestamp(get_sys_count());
    int queue_index = tsn_select_queue(now);
    if (queue_index < 0) {
        return NULL;
    }

    return tsn_queue_dequeue(queue_index);
}

static int transmit_tsn_packet_no_free(struct tsn_tx_buffer* packet) {

    uint64_t mem_len = sizeof(packet->metadata) + packet->metadata.frame_length;
    uint64_t bytes_tr;
    int status = 0;

    if (mem_len >= MAX_BUFFER_LENGTH) {
        printf("%s - %p length(%ld) is out of range(%d)\r\n", __func__, packet, mem_len, MAX_BUFFER_LENGTH);
        return XST_FAILURE;
    }

    status = xdma_api_write_from_buffer_with_fd(tx_devname, tx_fd, (char *)packet,
                                       mem_len, &bytes_tr);

    if(status != XST_SUCCESS) {
        tx_stats.txErrors++;
    } else {
        tx_stats.txPackets++;
        tx_stats.txBytes += bytes_tr;
    }

    return status;
}

static int transmit_tsn_packet(struct tsn_tx_buffer* packet) {

    int status = 0;

    status = transmit_tsn_packet_no_free(packet);

    buffer_pool_free((BUF_POINTER)packet);

    return status;
}

int extern_transmit_tsn_packet(struct tsn_tx_buffer* packet) {
    return transmit_tsn_packet(packet);
}

#ifndef ONE_QUEUE_TSN
static int enqueue(struct tsn_tx_buffer* tx) {
#ifndef DISABLE_TSN_QUEUE
    uint16_t prio = tx->metadata.vlan_prio;
    int res = tsn_queue_enqueue(tx, prio);
    if (res == 0) {
        buffer_pool_free((BUF_POINTER)tx);
    }
    return res;
#else
    transmit_tsn_packet(tx);
    return 1;
#endif
}

static int process_packet(struct tsn_rx_buffer* rx) {

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
#ifndef DISABLE_GPTP
    case ETH_TYPE_PTPv2:
        ;
        int len = process_gptp_packet(rx);
        if (len <= 0) { return XST_FAILURE; }
        tx_len += len;
        break;
#endif
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
    if (enqueue(tx) > 0) {
        return XST_SUCCESS;
    }
    return XST_FAILURE;
}
#endif

#ifdef ONE_QUEUE_TSN
char* g_tx_buffer;
#endif
static int process_send_packet(struct tsn_rx_buffer* rx) {

#ifdef ONE_QUEUE_TSN
    int tx_len;
    struct tsn_tx_buffer* tx = (struct tsn_tx_buffer*)(g_tx_buffer);
    struct tx_metadata* tx_metadata = &tx->metadata;
    uint8_t* rx_frame = (uint8_t*)&rx->data;
    uint8_t* tx_frame = (uint8_t*)&tx->data;
    struct ethernet_header* rx_eth = (struct ethernet_header*)rx_frame;
    struct ethernet_header* tx_eth = (struct ethernet_header*)tx_frame;

    // make ethernet frame
    memcpy(&(tx_eth->dmac), &(rx_eth->smac), 6);
    memcpy(&(tx_eth->smac), myMAC, 6);
    tx_eth->type = rx_eth->type;

    tx_len = ETH_HLEN;

    // do arp, udp echo, etc.
    switch (rx_eth->type) {
    case ETH_TYPE_ARP: // arp
        ;
        struct arp_header* rx_arp = (struct arp_header*)ETH_PAYLOAD(rx_frame);
        struct arp_header* tx_arp = (struct arp_header*)ETH_PAYLOAD(tx_frame);
        if (rx_arp->opcode != ARP_OPCODE_ARP_REQUEST) { return XST_FAILURE; }

        // make arp packet
        tx_arp->hw_type = rx_arp->hw_type;
        tx_arp->proto_type = rx_arp->proto_type;
        tx_arp->hw_size = rx_arp->hw_size;
        tx_arp->proto_size = rx_arp->proto_size;
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
        memcpy(tx_ipv4, rx_ipv4, IPv4_HLEN(rx_ipv4));
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
            memcpy(tx_icmp, rx_icmp, icmp_len);
            tx_icmp->type = ICMP_TYPE_ECHO_REPLY;
            icmp_checksum(tx_icmp, icmp_len);
            tx_len += icmp_len;

        } else if (rx_ipv4->proto == IP_PROTO_UDP){
            struct udp_header* rx_udp = (struct udp_header*)IPv4_PAYLOAD(rx_ipv4);
            if (rx_udp->dstport != 7) { return XST_FAILURE; }

            struct udp_header* tx_udp = IPv4_PAYLOAD(tx_ipv4);

            // Fill UDP header
            memcpy(tx_udp, rx_udp, rx_udp->length);
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

    // make tx metadata
    tx_metadata->timestamp_id = 0;
    tx_metadata->fail_policy = 0;
    memset(tx_metadata->reserved0, 0, 3);
    tx_metadata->reserved1 = 0;
    tx_metadata->reserved2 = 0;

    uint64_t now = get_sys_count();
    tx_metadata->from.tick = (uint32_t)((now + XDMA_SECTION_TAKEN_TICKS) & 0x1FFFFFFF);
    tx_metadata->to.tick = (uint32_t)((now + XDMA_SECTION_TAKEN_TICKS + XDMA_SECTION_TICKS_MARGIN) & 0x1FFFFFFF);
//    tx_metadata->delay_from.tick = (uint32_t)((now + DELAY_TICKS) & 0x1FFFFFFF);
//    tx_metadata->delay_to.tick = (uint32_t)((now + DELAY_TICKS + DELAY_TICKS_MARGIN) & 0x1FFFFFFF);

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
#ifndef DISABLE_GPTP
    case ETH_TYPE_PTPv2:
        ;
        int len = process_gptp_packet(rx);
        if (len <= 0) { return XST_FAILURE; }
        tx_len += len;
        break;
#endif
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
#endif
    tx_metadata->frame_length = tx_len;
    transmit_tsn_packet_no_free(tx); 
    return XST_SUCCESS;
}

#ifndef ONE_QUEUE_TSN
static void periodic_process_ptp()
{
#ifdef DISABLE_GPTP
    return;
#endif
    if (1) {
        struct gptp_statistics_result stats[3];
        gptp_get_statistics(stats);

        struct gptp_statistics_result* offset = &stats[0];
        struct gptp_statistics_result* freq = &stats[1];
        struct gptp_statistics_result* delay = &stats[2];

        timestamp_t now = gptp_get_timestamp(get_sys_count());

        if (offset->num + freq->num + delay->num > 0) {
            printf_debug(
                "[%d.%03d]: "
                "rms %4d max %4d "
                "freq %+6d +/- %3d "
                "delay %5d +/- %3d\n",
                (int)(now / 1e9), (int)(now / 1e6) % 1000,
                offset->rms, offset->max_abs,
                freq->mean, freq->stdev,
                delay->mean, delay->stdev);
        }

        gptp_reset_statistics();
    }


    uint32_t size;
    struct tsn_tx_buffer* buffer;

    buffer = (struct tsn_tx_buffer*)get_reserved_tx_buffer();
    if (buffer == NULL) {
        printf_debug("Cannot get buffer for pdelay_req\n");
        return;
    }
    size = gptp_make_pdelay_req_packet(buffer);
    if (size == 0) {
        buffer_pool_free((BUF_POINTER)buffer);
        return;
    }
    enqueue(buffer);

    // gPTP Master part

    // Send announce packet
    buffer = (struct tsn_tx_buffer*)get_reserved_tx_buffer();
    if (buffer == NULL) {
        printf_debug("Cannot get buffer for announce\n");
        return;
    }
    size = gptp_make_announce_packet(buffer);
    if (size == 0) {
        buffer_pool_free((BUF_POINTER)buffer);
        return; // I am not a master
    }
    enqueue(buffer);

    // Send Sync packet
    buffer = (struct tsn_tx_buffer*)get_reserved_tx_buffer();
    if (buffer == NULL) {
        printf_debug("Cannot get buffer for sync\n");
        return;
    }
    size = gptp_make_sync_packet(buffer);
    transmit_tsn_packet(buffer);

    // Send Followup packet
    buffer = (struct tsn_tx_buffer*)get_reserved_tx_buffer();
    if (buffer == NULL) {
        printf_debug("Cannot get buffer for fup\n");
        return;
    }
    size = gptp_make_followup_packet(buffer);
    enqueue(buffer);
}

static void sender_in_tsn_mode(char* devname, int fd, uint64_t size) {

    QueueElement buffer = NULL;
    int received_packet_count;
    int index;
    int status;
    uint64_t last_timer = 0;

    while (tx_thread_run) {
         uint64_t now = get_sys_count();
        // Might need to be changed into get_timestamp from gPTP module
        if ((now - last_timer) > (1000000000 / 8)) {
            periodic_process_ptp();
            last_timer = now;
        }

        // Process RX
        received_packet_count = getQueueCount();

        if (received_packet_count > 0) {
            if(received_packet_count > 16) {
                received_packet_count = 16;
            }
            for(index=0; index<received_packet_count; index++) {
                buffer = NULL;
                buffer = xbuffer_dequeue();
                if(buffer == NULL) {
                    continue;
                }
                status = process_packet((struct tsn_rx_buffer*)buffer);
                if(status == XST_FAILURE) {
                    tx_stats.txFiltered++;
                    buffer_pool_free((BUF_POINTER)buffer);
                }
            }
        }
#ifndef DISABLE_TSN_QUEUE
        // Process TX
        for (int i = 0; i < 20; i += 1) {
            struct tsn_tx_buffer* tx_buffer = dequeue();
            if (tx_buffer == NULL) {
                break;
            }
            transmit_tsn_packet(tx_buffer);
        }
#endif
    }
}
#endif

static void sender_in_normal_mode(char* devname, int fd, uint64_t size) {

    QueueElement buffer = NULL;
    int status;

#ifdef ONE_QUEUE_TSN
    g_tx_buffer = NULL;
    if(posix_memalign((void **)&g_tx_buffer, BUFFER_ALIGNMENT /*alignment */, MAX_BUFFER_LENGTH + BUFFER_ALIGNMENT)) {
        fprintf(stderr, "%s - OOM %u.\n", __func__, MAX_BUFFER_LENGTH + BUFFER_ALIGNMENT);
        return;
    }
    memset(g_tx_buffer, 0, MAX_BUFFER_LENGTH);
#endif

    while (tx_thread_run) {
        buffer = NULL;
        buffer = xbuffer_dequeue();
        if(buffer == NULL) {
            continue;
        }

        status = process_send_packet((struct tsn_rx_buffer*)buffer);
        if(status == XST_FAILURE) {
            tx_stats.txFiltered++;
        }

        buffer_pool_free((BUF_POINTER)buffer);
    }

#ifdef ONE_QUEUE_TSN
    if(g_tx_buffer != NULL) {
        free(g_tx_buffer);
    }
#endif
}

#ifndef ONE_QUEUE_TSN
static void sender_in_performance_mode(char* devname, int fd, char *fn, uint64_t size) {

    QueueElement buffer = NULL;
    uint64_t bytes_tr;
    int infile_fd = -1;
    ssize_t rc;
    FILE *fp = NULL;

    printf(">>> %s\n", __func__);

    fp = fopen(fn, "rb");
    if(fp == NULL) {
        printf("Unable to open input file %s, %d.\n", fn, infile_fd);
        return;
    }

    buffer = (QueueElement)xdma_api_get_buffer(size);
    if(buffer == NULL) {
        close(infile_fd);
        return;
    }

    rc = fread(buffer, sizeof(char), size, fp);
    fclose(fp);
    if (rc < 0 || rc < size) {
        free(buffer);
        return;
    }

    while (tx_thread_run) {
        if(xdma_api_write_from_buffer_with_fd(devname, fd, buffer,
                                       size, &bytes_tr)) {
            tx_stats.txErrors++;
            continue;
        }

        tx_stats.txPackets++;
        tx_stats.txBytes += bytes_tr;
    }

    free(buffer);
}

static void sender_in_debug_mode(char* devname, int fd, char *fn, uint64_t size) {

    QueueElement buffer = NULL;
    uint64_t bytes_tr;
    int infile_fd = -1;
    ssize_t rc;
    FILE *fp = NULL;

    printf(">>> %s\n", __func__);

    fp = fopen(fn, "rb");
    if(fp == NULL) {
        printf("Unable to open input file %s, %d.\n", fn, infile_fd);
        return;
    }

    if(posix_memalign((void **)&buffer, BUFFER_ALIGNMENT /*alignment */, MAX_BUFFER_LENGTH + BUFFER_ALIGNMENT)) {
        fprintf(stderr, "OOM %u.\n", MAX_BUFFER_LENGTH + BUFFER_ALIGNMENT);
        return;
    }

    for(int i=0; i<=MAX_PACKET_BURST; i++) {
        fseek( fp, 0, SEEK_SET );
        rc = fread((QueueElement)&buffer[i*MAX_PACKET_LENGTH], sizeof(char), size, fp);
        if (rc < 0 || rc < size) {
            free(buffer);
            fclose(fp);
            return;
        }
    }

    fclose(fp);

    while (tx_thread_run) {
        if(xdma_api_write_from_buffer_with_fd(devname, fd, buffer,
                                       MAX_BUFFER_LENGTH, &bytes_tr)) {
            tx_stats.txErrors++;
            continue;
        }

        tx_stats.txPackets += MAX_PACKET_BURST;
        tx_stats.txBytes += bytes_tr;

        tx_thread_run = 0;
    }

    free(buffer);
}
#endif

void* sender_thread(void* arg) {

    tx_thread_arg_t* p_arg = (tx_thread_arg_t*)arg;

    printf(">>> %s(devname: %s, fn: %s, mode: %d, size: %d)\n", 
               __func__, p_arg->devname, p_arg->fn, 
               p_arg->mode, p_arg->size);

    memset(tx_devname, 0, MAX_DEVICE_NAME);
    memcpy(tx_devname, p_arg->devname, MAX_DEVICE_NAME);

    if(xdma_api_dev_open(p_arg->devname, 0 /* eop_flush */, &tx_fd)) {
        printf("FAILURE: Could not open %s. Make sure xdma device driver is loaded and you have access rights (maybe use sudo?).\n", p_arg->devname);
        printf("<<< %s\n", __func__);
        return NULL;
    }

    initialize_statistics(&tx_stats);

    switch(p_arg->mode) {
#ifdef ONE_QUEUE_TSN
    case RUN_MODE_NORMAL:
        sender_in_normal_mode(p_arg->devname, tx_fd, p_arg->size);
    break;
#else
    case RUN_MODE_TSN:
        sender_in_tsn_mode(p_arg->devname, tx_fd, p_arg->size);
    break;
    case RUN_MODE_NORMAL:
        sender_in_normal_mode(p_arg->devname, tx_fd, p_arg->size);
    break;
    case RUN_MODE_LOOPBACK:
    case RUN_MODE_PERFORMANCE:
        sender_in_performance_mode(p_arg->devname, tx_fd, p_arg->fn, p_arg->size);
    break;
    case RUN_MODE_DEBUG:
        sender_in_debug_mode(p_arg->devname, tx_fd, p_arg->fn, p_arg->size);
    break;
#endif
    default:
        printf("%s - Unknown mode(%d)\n", __func__, p_arg->mode);
    break;
    }

    close(tx_fd);
    printf("<<< %s()\n", __func__);

    return NULL;
}

#ifdef ONE_QUEUE_TSN

#define DEFAULT_PKT_LEN 74
#define TOTAL_PKT_LEN 78

void dump_buffer(unsigned char* buffer, int len) {

    for(int idx = 0; idx < len; idx++) {
        if((idx % 16) == 0) {
            printf("\n  ");
        }
        printf("0x%02x ", buffer[idx] & 0xFF);
    }
    printf("\n");
}

void dump_tsn_tx_buffer(struct tsn_tx_buffer* packet, int len) {

#if 1
    printf(" 0x%08x  0x%08x ", 
        packet->metadata.from.tick, packet->metadata.to.tick);
    printf("   0x%08x  0x%08x ", 
        packet->metadata.delay_from.tick, packet->metadata.delay_to.tick);
    printf("   %4d        %1d ", 
        packet->metadata.frame_length, packet->metadata.fail_policy);
#else
    printf("[tx_metadata]\n");
    printf("        from.tick: 0x%08x,         to.tick: 0x%08x\n", 
        packet->metadata.from.tick, packet->metadata.to.tick);
    printf("  delay_from.tick: 0x%08x,   delay_to.tick: 0x%08x\n", 
        packet->metadata.delay_from.tick, packet->metadata.delay_to.tick);
    printf("     frame_length: %10d,     fail_policy: %10d\n", 
        packet->metadata.frame_length, packet->metadata.fail_policy);
    printf("[packet_data]");
    dump_buffer((unsigned char*)packet->data, (int)(len - sizeof(struct tx_metadata)));
    printf("\n");
#endif
}

static inline void wait_tick_count_almost_full(uint32_t h_count, uint32_t l_count) {
    uint32_t sys_count;

    sys_count = (uint32_t)(get_register(REG_SYS_COUNT_LOW) & 0x1FFFFFFF);
    while(((0x1FFFFFFF - sys_count) > h_count) || ((0x1FFFFFFF - sys_count) < l_count) ) {
        sys_count = (uint32_t)(get_register(REG_SYS_COUNT_LOW) & 0x1FFFFFFF);
    }
}

void fill_packet_data_with_default_packet(struct tsn_tx_buffer* packet, uint8_t stuff) {

    uint8_t default_packet[DEFAULT_PKT_LEN] = {
        0xa4, 0xbf, 0x01, 0x65, 0xde, 0x83, 0xd8, 0xbb, 0xc1, 0x15, 0x66, 0xc1, 0x08, 0x00, 0x45, 0x00,
        0x00, 0x3c, 0xce, 0x38, 0x00, 0x00, 0x80, 0x01, 0x60, 0x9e, 0xc0, 0xa8, 0x0a, 0x64, 0xc0, 0xa8,
        0x45, 0x36, 0x08, 0x00, 0x4c, 0x75, 0x00, 0x01, 0x00, 0xe6, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
        0x77, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69 };

    if(stuff == 0 ) {
        memcpy(packet->data, default_packet, DEFAULT_PKT_LEN);
    } else {
        memcpy(packet->data, default_packet, 42);
        for(int idx = 42; idx < DEFAULT_PKT_LEN; idx++) {
            packet->data[idx] = stuff & 0xFF;
        }
    }
}

#define TICK_COUNT_ALMOST_FULL_H (10000)
#define TICK_COUNT_ALMOST_FULL_L (0)

void fill_tx_metadata(struct tx_metadata* tx_metadata, int FP, int FT_OF, int DF_OF, uint16_t f_len) {

    tx_metadata->timestamp_id = 0;
    tx_metadata->fail_policy = (uint8_t)(FP & 0x1);
    tx_metadata->frame_length = f_len;

    if(((FT_OF & 0x1) == 0) && ((DF_OF & 0x1) == 0)) {
        uint64_t now = get_sys_count();
        tx_metadata->from.tick = (uint32_t)((now + 1000000) & 0x1FFFFFFF);
        tx_metadata->to.tick = (uint32_t)((now + 1500000) & 0x1FFFFFFF);
        tx_metadata->delay_from.tick = (uint32_t)((now + 3000000) & 0x1FFFFFFF);
        tx_metadata->delay_to.tick = (uint32_t)((now + 3500000) & 0x1FFFFFFF);
    } else if(((FT_OF & 0x1) == 0) && ((DF_OF & 0x1) == 1)) {
        wait_tick_count_almost_full(TICK_COUNT_ALMOST_FULL_H, TICK_COUNT_ALMOST_FULL_L);
        tx_metadata->from.tick = (uint32_t)(0x1fffffe0);
        tx_metadata->to.tick = (uint32_t)(0x1fffffef);
        tx_metadata->delay_from.tick = (uint32_t)(0x000000e0);
        tx_metadata->delay_to.tick = (uint32_t)(0x1fffffff);
    } else if(((FT_OF & 0x1) == 1) && ((DF_OF & 0x1) == 0)) {
        tx_metadata->from.tick = (uint32_t)(0x1ffffff1);
        tx_metadata->to.tick = (uint32_t)(0x00000000);
        tx_metadata->delay_from.tick = (uint32_t)(0x1fffffff);
        tx_metadata->delay_to.tick = (uint32_t)(0x0000000e);
    } else if(((FT_OF & 0x1) == 1) && ((DF_OF & 0x1) == 1)) {
        tx_metadata->from.tick = (uint32_t)(0x1ffffff1);
        tx_metadata->to.tick = (uint32_t)(0x00000000);
        tx_metadata->delay_from.tick = (uint32_t)(0x000000f1);
        tx_metadata->delay_to.tick = (uint32_t)(0x00000100);
    }
}

void make_tsn_tx_buffer(struct tsn_tx_buffer* packet, uint8_t stuff) {
    struct tsn_tx_buffer* tx = (struct tsn_tx_buffer*)(packet);
    struct tx_metadata* tx_metadata = &tx->metadata;

    memset(packet, 0, sizeof(struct tsn_tx_buffer));

    fill_packet_data_with_default_packet(packet, stuff);
}

int tr_packet_dump_buffer(struct tsn_tx_buffer* packet) {

    transmit_tsn_packet_no_free(packet);
    dump_tsn_tx_buffer(packet, (int)(sizeof(struct tx_metadata) + packet->metadata.frame_length));
    return XST_SUCCESS;
}

int simple_test_case_with_id(char* ip_address, uint32_t from_tick, uint32_t margin, uint32_t id) {
    struct tsn_tx_buffer packet;

    make_tsn_tx_buffer(&packet, 0x00);
    switch(id) {
    case 1: fill_tx_metadata(&packet.metadata, 0, 0, 0, TOTAL_PKT_LEN); break;
    case 2: fill_tx_metadata(&packet.metadata, 0, 0, 1, TOTAL_PKT_LEN); break;
    case 3: fill_tx_metadata(&packet.metadata, 0, 1, 0, TOTAL_PKT_LEN); break;
    case 4: fill_tx_metadata(&packet.metadata, 0, 1, 1, TOTAL_PKT_LEN); break;
    case 5: fill_tx_metadata(&packet.metadata, 1, 0, 0, TOTAL_PKT_LEN); break;
    case 6: fill_tx_metadata(&packet.metadata, 1, 0, 1, TOTAL_PKT_LEN); break;
    case 7: fill_tx_metadata(&packet.metadata, 1, 1, 0, TOTAL_PKT_LEN); break;
    case 8: fill_tx_metadata(&packet.metadata, 1, 1, 1, TOTAL_PKT_LEN); break;
    default:
        printf("%s - Unknown test ID(%d)\n", __func__, id);
        return -1;
    }

    return tr_packet_dump_buffer(&packet);
}

void fill_tx_metadata_except_from_to(struct tsn_tx_buffer* packet, uint8_t stuff, 
             uint16_t frame_length, uint16_t timestamp_id, uint8_t fail_policy) {
    struct tx_metadata* tx_metadata = &packet->metadata;

    make_tsn_tx_buffer(packet, stuff);

    // make tx metadata
    tx_metadata->timestamp_id = timestamp_id;
    tx_metadata->fail_policy = fail_policy;
    tx_metadata->frame_length = frame_length;
}

#define PACKET_BURST_SIZE (256)

int test_with_n_packets(char* ip_address, uint32_t from_tick, uint32_t margin) {
    struct tsn_tx_buffer packet[PACKET_BURST_SIZE];
    struct tsn_tx_buffer packet2;
    struct tx_metadata* tx_metadata[PACKET_BURST_SIZE];
    int id;

    for(id=0; id<PACKET_BURST_SIZE; id++) {
        tx_metadata[id] = &packet[id].metadata;
        fill_tx_metadata_except_from_to(&packet[id], (uint8_t)(id & 0xFF), TOTAL_PKT_LEN, 0, 0);
    }


    uint64_t now = get_sys_count();
    for(id=0; id<PACKET_BURST_SIZE; id++) {
        tx_metadata[id]->from.tick = (uint32_t)((now + 1000000 + id * 1000) & 0x1FFFFFFF);
        tx_metadata[id]->to.tick = (uint32_t)((now + 1005000 + id * 1000) & 0x1FFFFFFF);
    }

    for(id=0; id<PACKET_BURST_SIZE; id++) {
        transmit_tsn_packet_no_free(&packet[id]);
    }
    return XST_SUCCESS;
}

int test_with_two_packets(char* ip_address, uint32_t from_tick, uint32_t margin) {
    struct tsn_tx_buffer packet;
    struct tsn_tx_buffer packet2;
    struct tx_metadata* tx_metadata = &packet.metadata;
    struct tx_metadata* tx_metadata2 = &packet2.metadata;

    fill_tx_metadata_except_from_to(&packet, 0x11, TOTAL_PKT_LEN, 0, 0);
    fill_tx_metadata_except_from_to(&packet2, 0x22, TOTAL_PKT_LEN, 0, 0);

    uint64_t now = get_sys_count();
    tx_metadata->from.tick = (uint32_t)((now + 25000) & 0x1FFFFFFF);
    tx_metadata->to.tick = (uint32_t)((now + 27000) & 0x1FFFFFFF);
    tx_metadata2->from.tick = (uint32_t)((now + 26000) & 0x1FFFFFFF);
    tx_metadata2->to.tick = (uint32_t)((now + 28000) & 0x1FFFFFFF);

    transmit_tsn_packet_no_free(&packet);
    transmit_tsn_packet_no_free(&packet2);
    return XST_SUCCESS;
}

/* Test Case #999 */
int test_count_is_bigger_than_to_fp_1(char* ip_address, uint32_t from_tick, uint32_t margin) {
    struct tsn_tx_buffer packet;
    struct tx_metadata* tx_metadata = &packet.metadata;

    make_tsn_tx_buffer(&packet, 0x00);

    // make tx metadata
    tx_metadata->timestamp_id = 0;
    tx_metadata->fail_policy = 1;

    uint64_t now = get_sys_count();
    tx_metadata->from.tick = (uint32_t)((now -1000) & 0x1FFFFFFF);
    tx_metadata->to.tick = (uint32_t)((now) & 0x1FFFFFFF);
    tx_metadata->delay_from.tick = (uint32_t)((now + 15000) & 0x1FFFFFFF);
    tx_metadata->delay_to.tick = (uint32_t)((now + 25000) & 0x1FFFFFFF);

    tx_metadata->frame_length = TOTAL_PKT_LEN;
    transmit_tsn_packet_no_free(&packet);
#if 0
    printf(" 0x%08x ", (uint32_t)(now & 0x1FFFFFFF));
    dump_tsn_tx_buffer(&packet, (int)(sizeof(struct tx_metadata) + tx_metadata->frame_length));
#endif
    return XST_SUCCESS;
}

int test_fail_policy(char* ip_address, uint32_t from_tick, uint32_t margin) {
    struct tsn_tx_buffer packet;
    struct tx_metadata* tx_metadata = &packet.metadata;

    make_tsn_tx_buffer(&packet, 0x00);

    // make tx metadata
    tx_metadata->timestamp_id = 0;
    tx_metadata->fail_policy = 1;
    tx_metadata->frame_length = TOTAL_PKT_LEN;

    uint64_t now = get_sys_count();
    tx_metadata->from.tick = (uint32_t)((now - 100) & 0x1FFFFFFF);
    tx_metadata->to.tick = (uint32_t)((now + 0) & 0x1FFFFFFF);
    tx_metadata->delay_from.tick = (uint32_t)((now + 115000) & 0x1FFFFFFF);
    //tx_metadata->delay_to.tick = (uint32_t)((now + 139100) & 0x1FFFFFFF);
    tx_metadata->delay_to.tick = (uint32_t)(0x1FFFFFFF);

    transmit_tsn_packet_no_free(&packet);
    printf(" 0x%08x ", (uint32_t)(now & 0x1FFFFFFF));
    dump_tsn_tx_buffer(&packet, (int)(sizeof(struct tx_metadata) + tx_metadata->frame_length));
    return XST_SUCCESS;
}

int test_from_bigger_than_to(char* ip_address, uint32_t from_tick, uint32_t margin) {
    struct tsn_tx_buffer packet;
    struct tx_metadata* tx_metadata = &packet.metadata;

    make_tsn_tx_buffer(&packet, 0x00);

    // make tx metadata
    tx_metadata->timestamp_id = 0;
    tx_metadata->fail_policy = 0;
    tx_metadata->frame_length = TOTAL_PKT_LEN;

    wait_tick_count_almost_full(TICK_COUNT_ALMOST_FULL_H, TICK_COUNT_ALMOST_FULL_L);
    uint64_t now = get_sys_count();
    tx_metadata->from.tick = (uint32_t)((now + 0) & 0x1FFFFFFF);
    tx_metadata->to.tick = (uint32_t)((now + 19100) & 0x1FFFFFFF);
//    tx_metadata->delay_from.tick = (uint32_t)((now + 3000000) & 0x1FFFFFFF);
//    tx_metadata->delay_to.tick = (uint32_t)((now + 3500000) & 0x1FFFFFFF);

    transmit_tsn_packet_no_free(&packet);
    printf(" 0x%08x ", (uint32_t)(now & 0x1FFFFFFF));
    dump_tsn_tx_buffer(&packet, (int)(sizeof(struct tx_metadata) + tx_metadata->frame_length));
    return XST_SUCCESS;
}

#define ONE_QUEUE_TSN_DEBUG 0

uint32_t from_bigger_than_to_count = 0;
int find_tick_count_delay(char* ip_address, uint32_t from_tick, uint32_t margin) {
    struct tsn_tx_buffer packet;
    struct tx_metadata* tx_metadata = &packet.metadata;

    make_tsn_tx_buffer(&packet, 0x00);

    // make tx metadata
    tx_metadata->timestamp_id = 0;
    tx_metadata->fail_policy = 0;
    tx_metadata->frame_length = TOTAL_PKT_LEN;

    uint64_t now = get_sys_count();
    //tx_metadata->from.tick = (uint32_t)((now + 1000) & 0x1FFFFFFF);
    tx_metadata->from.tick = (uint32_t)((now) & 0x1FFFFFFF);
//    tx_metadata->from.tick = (uint32_t)((now + 500) & 0x1FFFFFFF);
    //tx_metadata->to.tick = (uint32_t)(0x1FFFFFFF);
    tx_metadata->to.tick = (uint32_t)((now + 3800) & 0x1FFFFFFF);
//    tx_metadata->to.tick = (uint32_t)((now + 19100) & 0x1FFFFFFF);
//    tx_metadata->delay_from.tick = (uint32_t)(0x1FFFF800);
//    tx_metadata->delay_to.tick = (uint32_t)(0x1FFFFFFF);

    transmit_tsn_packet_no_free(&packet);
#if 1
#if ONE_QUEUE_TSN_DEBUG
    printf(" 0x%08x ", (uint32_t)(now & 0x1FFFFFFF));
    dump_tsn_tx_buffer(&packet, (int)(sizeof(struct tx_metadata) + tx_metadata->frame_length));
#endif
    if(tx_metadata->from.tick > tx_metadata->to.tick) {
        from_bigger_than_to_count++;
    }
#else
    dump_tsn_tx_buffer(&packet, (int)(sizeof(struct tx_metadata) + tx_metadata->frame_length));
    printf("\n[raw data]");
    dump_buffer((unsigned char*)&packet, (int)(tx_metadata->frame_length + sizeof(struct tx_metadata)));
    printf("get_sys_count[31..0]: 0x%08x\n", (uint32_t)(now & 0x1FFFFFFF));
    printf("<<< %s()\n", __func__);
#endif
    return XST_SUCCESS;
}

int long_test_with_found_tick_count(int count) {
    struct tsn_tx_buffer packet;
    struct tx_metadata* tx_metadata = &packet.metadata;

    make_tsn_tx_buffer(&packet, 0x00);

    // make tx metadata
    tx_metadata->timestamp_id = 0;
    tx_metadata->fail_policy = 0;
    tx_metadata->frame_length = TOTAL_PKT_LEN;

    for(int id = 0; id <count; id++) {
        uint64_t now = get_sys_count();
        tx_metadata->from.tick = (uint32_t)((now + 500) & 0x1FFFFFFF);
        tx_metadata->to.tick = (uint32_t)((now + 19100) & 0x1FFFFFFF);

        transmit_tsn_packet_no_free(&packet);

        if(tx_metadata->from.tick > tx_metadata->to.tick) {
            from_bigger_than_to_count++;
        }
    }
    return XST_SUCCESS;
}

uint32_t tx_packets_zero_count = 0;
uint32_t tx_packets = 0;
uint32_t tx_input_packet_counter = 0;
uint32_t tx_output_packet_counter = 0;

#if ONE_QUEUE_TSN_DEBUG
#define TEST_PACKET_COUNT (500)
//#define TEST_PACKET_COUNT (20)
#else
#define TEST_PACKET_COUNT (800000000)
//#define TEST_PACKET_COUNT (1000000)
#endif
int send_1queueTSN_packet(char* ip_address, uint32_t from_tick, uint32_t margin) {

    memset(tx_devname, 0, MAX_DEVICE_NAME);
    memcpy(tx_devname, DEF_TX_DEVICE_NAME, sizeof(DEF_TX_DEVICE_NAME));

    if(xdma_api_dev_open(DEF_TX_DEVICE_NAME, 0 /* eop_flush */, &tx_fd)) {
        printf("FAILURE: Could not open %s. Make sure xdma device driver is loaded and you have access rights (maybe use sudo?).\n", DEF_TX_DEVICE_NAME);
        printf("<<< %s\n", __func__);
        return -1;
    }

    dump_registers(DUMPREG_TX, 1);
    tx_packets += get_register(REG_TX_PACKETS);
    tx_packets = 0;
    tx_input_packet_counter += get_register(REG_TX_INPUT_PACKET_COUNT);
    tx_output_packet_counter += get_register(REG_TX_OUTPUT_PACKET_COUNT);

    set_register(REG_TSN_CONTROL, 1);
#if ONE_QUEUE_TSN_DEBUG
    printf("\n[sys_count] [from.tick] [  to.tick] [d_from.tick] [d_to.tick] "); 
    printf("[length] [policy] [tx packets] [tx input packet] [tx output packet]\n");  
#endif
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    long_test_with_found_tick_count(TEST_PACKET_COUNT);
#if 0
    for(int id = 0; id < TEST_PACKET_COUNT; id++) {
//        test_with_two_packets(ip_address, from_tick, margin);
//        test_with_n_packets(ip_address, from_tick, margin);
        find_tick_count_delay(ip_address, from_tick, margin);
//        test_fail_policy(ip_address, from_tick, margin);
//        test_count_is_bigger_than_to_fp_1(* ip_address, from_tick, margin);
//        test_from_bigger_than_to(ip_address, from_tick, margin);
#if ONE_QUEUE_TSN_DEBUG
        sleep(1);
        printf("      %6d ", tx_packets = get_register(REG_TX_PACKETS));
        printf("     %12d ", get_register(REG_TX_INPUT_PACKET_COUNT));
        printf("      %12d\n", get_register(REG_TX_OUTPUT_PACKET_COUNT));
        if(tx_packets == 0) {
            tx_packets_zero_count++;
//            dump_registers(DUMPREG_TX, 1);
        }
#endif
    }
#endif
    gettimeofday(&end_time, NULL);
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long microseconds = end_time.tv_usec - start_time.tv_usec;
    double elapsed_time = seconds + (microseconds / 1000000.0);

    printf("\nElapsed time: %f seconds, from_bigger_than_to_count: %d, tx_packets_zero_count: %d\n", 
             elapsed_time, from_bigger_than_to_count, tx_packets_zero_count);

#if ONE_QUEUE_TSN_DEBUG
    printf("\n");
#endif
//    simple_test_case_with_id(ip_address, from_tick, margin, 1);
//    simple_test_case_with_id(ip_address, from_tick, margin, 2);
//    simple_test_case_with_id(ip_address, from_tick, margin, 3);
//    simple_test_case_with_id(ip_address, from_tick, margin, 4);
//    simple_test_case_with_id(ip_address, from_tick, margin, 5);
//    simple_test_case_with_id(ip_address, from_tick, margin, 6);
//    simple_test_case_with_id(ip_address, from_tick, margin, 7);
//    simple_test_case_with_id(ip_address, from_tick, margin, 8);

    close(tx_fd);
    sleep(1);

    dump_registers(DUMPREG_TX, 1);
    printf("<<< %s()\n", __func__);

    return XST_SUCCESS;
}

#endif
