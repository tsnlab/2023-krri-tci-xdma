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
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "platform_config.h"
#include "xdma_common.h"
#include "buffer_handler.h"

#include "../libxdma/api_xdma.h"
#include "../libxdma/ioctl_xdma.h"

static CircularQueue_t g_queue;
static CircularQueue_t* queue = NULL;

stats_t rx_stats;

extern int rx_thread_run;

void initialize_queue(CircularQueue_t* p_queue) {
    queue = p_queue;

    p_queue->front = 0;
    p_queue->rear = -1;
    p_queue->count = 0;
    pthread_mutex_init(&p_queue->mutex, NULL);
}

static int isQueueEmpty() {
    return (queue->count == 0);
}

static int isQueueFull() {
    return (queue->count == NUMBER_OF_QUEUE);
}

int getQueueCount() {
    return queue->count;
}

static void xbuffer_enqueue(QueueElement element) {
    pthread_mutex_lock(&queue->mutex);

    if (isQueueFull()) {
        debug_printf("Queue is full. Cannot xbuffer_enqueue.\n");
        pthread_mutex_unlock(&queue->mutex);
        return;
    }

    queue->rear = (queue->rear + 1) % NUMBER_OF_QUEUE;
    queue->elements[queue->rear] = element;
    queue->count++;

    pthread_mutex_unlock(&queue->mutex);
}

static QueueElement xbuffer_dequeue() {
    pthread_mutex_lock(&queue->mutex);

    if (isQueueEmpty()) {
        debug_printf("Queue is empty. Cannot xbuffer_dequeue.\n");
        pthread_mutex_unlock(&queue->mutex);
        return EMPTY_ELEMENT;
    }

    QueueElement dequeuedElement = queue->elements[queue->front];
    queue->front = (queue->front + 1) % NUMBER_OF_QUEUE;
    queue->count--;

    pthread_mutex_unlock(&queue->mutex);

    return dequeuedElement;
}

void initialize_statistics(stats_t* p_stats) {

    memset(p_stats, 0, sizeof(stats_t));
}

#define BUFFER_SIZE 16

void print_hex_ascii(FILE *fp, int addr, const unsigned char *buffer, size_t length) {

    size_t i, j;

    fprintf(fp, "%7d: ", addr);
    for (i = 0; i < length; i++) {
        fprintf(fp, "%02X ", buffer[i]);
        if (i == 7)
            fprintf(fp, " ");
    }
    if (length < BUFFER_SIZE) {
        for (j = 0; j < (BUFFER_SIZE - length); j++) {
            fprintf(fp, "   ");
            if (j == 7)
                fprintf(fp, " ");
        }
    }
    fprintf(fp, " ");
    for (i = 0; i < length; i++) {
        if (buffer[i] >= 32 && buffer[i] <= 126) {
            fprintf(fp, "%c", buffer[i]);
        } else {
            fprintf(fp, ".");
        }
        if (i == 7)
            fprintf(fp, " ");
    }
    fprintf(fp, "\n");
}

void packet_dump(FILE *fp, BUF_POINTER buffer, int length) {

    int total = 0, len;
    int address = 0;

    while(total < length) {
        if(length >= BUFFER_SIZE) {
            len = BUFFER_SIZE;
        } else {
            len = length;
        }
        print_hex_ascii(fp, address, (const unsigned char *)&buffer[total], len);
        total += len;
        address += BUFFER_SIZE;
    }
}

static void receiver_in_normal_mode(char* devname, int fd, uint64_t size) {

    BUF_POINTER buffer;
    int bytes_rcv;

    set_register(REG_TSN_CONTROL, 1);
    while (rx_thread_run) {
        buffer = buffer_pool_alloc();
        if(buffer == NULL) {
            debug_printf("FAILURE: Could not buffer_pool_alloc.\n");
            rx_stats.rxNoBuffer++;
            continue;
        }

        bytes_rcv = 0;
        if(xdma_api_read_to_buffer_with_fd(devname, fd, buffer, 
                                           size, &bytes_rcv)) {
            if(buffer_pool_free(buffer)) {
                debug_printf("FAILURE: Could not buffer_pool_free.\n");
            }
            rx_stats.rxErrors++;
            continue;
        }
        if(bytes_rcv > MAX_BUFFER_LENGTH) {
            if(buffer_pool_free(buffer)) {
                debug_printf("FAILURE: Could not buffer_pool_free.\n");
            }
            rx_stats.rxErrors++;
            continue;
        }
        rx_stats.rxPackets++;
        rx_stats.rxBytes += bytes_rcv;

        xbuffer_enqueue((QueueElement)buffer);
    }
    set_register(REG_TSN_CONTROL, 0);
}

#ifdef ONE_QUEUE_TSN
static void dump_rx_packet_to_file(unsigned char* buffer, int len) {

    FILE *fp;
    char *filename = "./rx_packet.txt";
    char pbuffer[16 * 3];

    fp = fopen(filename, "a+");
    if (fp == NULL) {
        printf("%s 파일을 열 수 없습니다.\n", filename);
        return;
    }

    int i = 0;
    fprintf(fp, "[Buffer]\n");

    while (i < len) {
        int j = 0;
        memset(pbuffer, 0, sizeof(pbuffer));
        while (j < 16) {
            if (i + j >= len) {
                break;
            }
            sprintf(pbuffer + (j * 3), "%02x ", buffer[i + j] & 0xFF);
            j++;
        }
        fprintf(fp, "%s\n", pbuffer);
        i += 16;
    }
    fprintf(fp, "\n");

    fclose(fp);
}

static void receiver_in_pcap_mode(char* devname, int fd, uint64_t size) {

    BUF_POINTER buffer;
    int bytes_rcv;
    struct tsn_rx_buffer* rx;

    set_register(REG_TSN_CONTROL, 1);
    while (rx_thread_run) {
        buffer = buffer_pool_alloc();
        if(buffer == NULL) {
            debug_printf("FAILURE: Could not buffer_pool_alloc.\n");
            rx_stats.rxNoBuffer++;
            continue;
        }

        bytes_rcv = 0;
        if(xdma_api_read_to_buffer_with_fd(devname, fd, buffer, 
                                           size, &bytes_rcv)) {
            if(buffer_pool_free(buffer)) {
                debug_printf("FAILURE: Could not buffer_pool_free.\n");
            }
            rx_stats.rxErrors++;
            continue;
        }
        if(bytes_rcv > MAX_BUFFER_LENGTH) {
            if(buffer_pool_free(buffer)) {
                debug_printf("FAILURE: Could not buffer_pool_free.\n");
            }
            rx_stats.rxErrors++;
            continue;
        }
        rx_stats.rxPackets++;
        rx_stats.rxBytes += bytes_rcv;

        rx = (struct tsn_rx_buffer*)buffer;
        dump_rx_packet_to_file((unsigned char*)rx->data, rx->metadata.frame_length);

        xbuffer_enqueue((QueueElement)buffer);
    }
    set_register(REG_TSN_CONTROL, 0);
}
#endif

void receiver_in_loopback_mode(char* devname, int fd, char *fn, uint64_t size) {

    BUF_POINTER buffer;
    BUF_POINTER data;
    int bytes_rcv;
    int infile_fd = -1;
    ssize_t rc;
    FILE *fp = NULL;

    printf(">>> %s\n", __func__);

    fp = fopen(fn, "rb");
    if(fp == NULL) {
        printf("Unable to open input file %s, %d.\n", fn, infile_fd);
        return;
    }

    data = (QueueElement)xdma_api_get_buffer(size);
    if(data == NULL) {
        close(infile_fd);
        return;
    }

    rc = fread(data, sizeof(char), size, fp);
    fclose(fp);
    if (rc < 0 || rc < size) {
        free(data);
        return;
    }

    buffer = buffer_pool_alloc();
    if(buffer == NULL) {
        printf("FAILURE: Could not buffer_pool_alloc.\n");
        return;
    }

    while (rx_thread_run) {

        if(xdma_api_read_to_buffer_with_fd(devname, fd, buffer, 
                                           size, &bytes_rcv)) {
            continue;
        }

        if(size != bytes_rcv) {
            debug_printf("FAILURE: size(%ld) and bytes_rcv(%ld) are different.\n", 
                         size, bytes_rcv);
            rx_stats.rxErrors++;
            continue;
        }

        if(memcmp((const void *)data, (const void *)buffer, size)) {
            debug_printf("FAILURE: data(%p) and buffer(%p) are different.\n", 
                         data, buffer);
            rx_stats.rxErrors++;
            continue;
        }

        rx_stats.rxPackets++;
        rx_stats.rxBytes += bytes_rcv;

    }
    buffer_pool_free(buffer);
}

void receiver_in_performance_mode(char* devname, int fd, char *fn, uint64_t size) {

    BUF_POINTER buffer;
    int bytes_rcv;

    set_register(REG_TSN_CONTROL, 1);
    while (rx_thread_run) {
        buffer = buffer_pool_alloc();
        if(buffer == NULL) {
            debug_printf("FAILURE: Could not buffer_pool_alloc.\n");
            continue;
        }

        if(xdma_api_read_to_buffer_with_fd(devname, fd, buffer, 
                                           size, &bytes_rcv)) {
            if(buffer_pool_free(buffer)) {
                debug_printf("FAILURE: Could not buffer_pool_free.\n");
            }
            continue;
        }

        if(size != bytes_rcv) {
            debug_printf("FAILURE: size(%ld) and bytes_rcv(%ld) are different.\n", 
                         size, bytes_rcv);
            if(buffer_pool_free(buffer)) {
                debug_printf("FAILURE: Could not buffer_pool_free.\n");
            }
            rx_stats.rxErrors++;
            continue;
        }

        rx_stats.rxPackets++;
        rx_stats.rxBytes += bytes_rcv;

        buffer_pool_free(buffer);
    }
    set_register(REG_TSN_CONTROL, 0);
}

void receiver_in_debug_mode(char* devname, int fd, char *fn, uint64_t size) {

    BUF_POINTER buffer;
    int bytes_rcv;

    printf(">>> %s\n", __func__);

    if(posix_memalign((void **)&buffer, BUFFER_ALIGNMENT /*alignment */, MAX_BUFFER_LENGTH)) {
        fprintf(stderr, "OOM %u.\n", MAX_BUFFER_LENGTH);
        return;
    }

    while (rx_thread_run) {
        if(xdma_api_read_to_buffer_with_fd(devname, fd, buffer, 
                                           MAX_BUFFER_LENGTH, &bytes_rcv)) {
            continue;
        }

        for(int i=0; i<MAX_PACKET_BURST; i++) {
//            packet_dump(stdout, (BUF_POINTER)&buffer[i*MAX_PACKET_LENGTH], 32);
        }

        rx_stats.rxPackets++;
        rx_stats.rxBytes += bytes_rcv;
    }
}

void* receiver_thread(void* arg) {

    rx_thread_arg_t* p_arg = (rx_thread_arg_t*)arg;
    int fd = 0;

    printf(">>> %s(devname: %s, fn: %s, mode: %d, size: %d)\n", 
                __func__, p_arg->devname, p_arg->fn,
                p_arg->mode, p_arg->size);

    if(xdma_api_dev_open(p_arg->devname, 1 /* eop_flush */, &fd)) {
        printf("FAILURE: Could not open %s. Make sure xdma device driver is loaded and you have access rights (maybe use sudo?).\n", p_arg->devname);
        printf("<<< %s\n", __func__);
        return NULL;
    }

    initialize_queue(&g_queue);
    initialize_statistics(&rx_stats);

    switch(p_arg->mode) {
    case RUN_MODE_TSN:
    case RUN_MODE_NORMAL:
        receiver_in_normal_mode(p_arg->devname, fd, p_arg->size);
    break;
    case RUN_MODE_LOOPBACK:
        receiver_in_loopback_mode(p_arg->devname, fd, p_arg->fn, p_arg->size);
    break;
    case RUN_MODE_PERFORMANCE:
        receiver_in_performance_mode(p_arg->devname, fd, p_arg->fn, p_arg->size);
    break;
    case RUN_MODE_DEBUG:
        receiver_in_debug_mode(p_arg->devname, fd, p_arg->fn, p_arg->size);
    break;
#ifdef ONE_QUEUE_TSN
    case RUN_MODE_PCAP:
        receiver_in_pcap_mode(p_arg->devname, fd, p_arg->size);
    break;
#endif
    default:
        printf("%s - Unknown mode(%d)\n", __func__, p_arg->mode);
    break;
    }

    pthread_mutex_destroy(&g_queue.mutex);

    xdma_api_dev_close(fd);
    printf("<<< %s\n", __func__);
    return NULL;
}

#include "./sender_thread.c"

