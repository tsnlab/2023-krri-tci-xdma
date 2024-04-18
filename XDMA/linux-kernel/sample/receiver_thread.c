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

#include "xdma_common.h"
#include "platform_config.h"
#include "buffer_handler.h"

#include "../libxdma/api_xdma.h"
#include "../libxdma/ioctl_xdma.h"

static CircularQueue_t g_queue[NUMBER_OF_ETH_PORT];
static CircularQueue_t* queue[NUMBER_OF_ETH_PORT];

stats_t rx_stats[NUMBER_OF_ETH_PORT];

extern int rx_thread_run;

void initialize_queue(int port, CircularQueue_t* p_queue) {
    queue[port] = p_queue;

    p_queue->front = 0;
    p_queue->rear = -1;
    p_queue->count = 0;
    pthread_mutex_init(&p_queue->mutex, NULL);
}

static int isQueueEmpty(int port) {
    return (queue[port]->count == 0);
}

static int isQueueFull(int port) {
    return (queue[port]->count == NUMBER_OF_QUEUE);
}

int getQueueCount(int port) {
    return queue[port]->count;
}

static void xbuffer_enqueue(int port, QueueElement element) {
    pthread_mutex_lock(&queue[port]->mutex);

    if (isQueueFull(port)) {
        debug_printf("Queue is full. Cannot xbuffer_enqueue.\n");
        pthread_mutex_unlock(&queue[port]->mutex);
        return;
    }

    queue[port]->rear = (queue[port]->rear + 1) % NUMBER_OF_QUEUE;
    queue[port]->elements[queue[port]->rear] = element;
    queue[port]->count++;

    pthread_mutex_unlock(&queue[port]->mutex);
}

static QueueElement xbuffer_dequeue(int port) {
    pthread_mutex_lock(&queue[port]->mutex);

    if (isQueueEmpty(port)) {
        debug_printf("Queue is empty. Cannot xbuffer_dequeue.\n");
        pthread_mutex_unlock(&queue[port]->mutex);
        return EMPTY_ELEMENT;
    }

    QueueElement dequeuedElement = queue[port]->elements[queue[port]->front];
    queue[port]->front = (queue[port]->front + 1) % NUMBER_OF_QUEUE;
    queue[port]->count--;

    pthread_mutex_unlock(&queue[port]->mutex);

    return dequeuedElement;
}

void initialize_statistics(stats_t* p_stats) {

    memset(p_stats, 0, sizeof(stats_t));
}

#if 0
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
#endif

static void receiver_in_normal_mode(char* devname, int fd, uint64_t size, int port) {

    BUF_POINTER buffer;
    int bytes_rcv;

    set_register(REG_TSN_CONTROL, 1);
    while (rx_thread_run) {
        buffer = buffer_pool_alloc(port);
        if(buffer == NULL) {
            debug_printf("FAILURE: Could not buffer_pool_alloc.\n");
            rx_stats[port].rxNoBuffer++;
            continue;
        }

        bytes_rcv = 0;
        if(xdma_api_read_to_buffer_with_fd(devname, fd, buffer, 
                                           size, &bytes_rcv)) {
            if(buffer_pool_free(port, buffer)) {
                debug_printf("FAILURE: Could not buffer_pool_free.\n");
            }
            rx_stats[port].rxErrors++;
            continue;
        }
        if(bytes_rcv > MAX_BUFFER_LENGTH) {
            if(buffer_pool_free(port, buffer)) {
                debug_printf("FAILURE: Could not buffer_pool_free.\n");
            }
            rx_stats[port].rxErrors++;
            continue;
        }
        rx_stats[port].rxPackets++;
        rx_stats[port].rxBytes += bytes_rcv;

        xbuffer_enqueue(port, (QueueElement)buffer);
    }
    set_register(REG_TSN_CONTROL, 0);
}

void* receiver_thread(void* arg) {

    rx_thread_arg_t* p_arg = (rx_thread_arg_t*)arg;
    int fd = 0;
    int port;

    printf(">>> %s(devname: %s, fn: %s, mode: %d, size: %d, port: %d)\n", 
                __func__, p_arg->devname, p_arg->fn,
                p_arg->mode, p_arg->size, p_arg->port);

    if(xdma_api_dev_open(p_arg->devname, 1 /* eop_flush */, &fd)) {
        printf("FAILURE: Could not open %s. Make sure xdma device driver is loaded and you have access rights (maybe use sudo?).\n", p_arg->devname);
        printf("<<< %s\n", __func__);
        return NULL;
    }

    port = p_arg->port;

    initialize_queue(port, &g_queue[port]);
    initialize_statistics(&rx_stats[port]);

    switch(p_arg->mode) {
    case RUN_MODE_NORMAL:
        receiver_in_normal_mode(p_arg->devname, fd, p_arg->size, p_arg->port);
    break;
    default:
        printf("%s - Unknown mode(%d)\n", __func__, p_arg->mode);
    break;
    }

    pthread_mutex_destroy(&g_queue[port].mutex);

    xdma_api_dev_close(fd);
    printf("<<< %s\n", __func__);
    return NULL;
}

#include "./sender_thread.c"

