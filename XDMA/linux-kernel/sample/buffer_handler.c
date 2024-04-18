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
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>

#include "xdma_common.h"
#include "platform_config.h"
#include "libxdma/api_xdma.h"

/* Stack operation for Rx & Tx buffer management */

static buffer_stack_t xdma_buffer_pool_stack[NUMBER_OF_ETH_PORT];
BUF_POINTER buffer_list[NUMBER_OF_ETH_PORT][NUMBER_OF_BUFFER];

static buffer_stack_t* stack[NUMBER_OF_ETH_PORT];

void relese_buffers(int port, int count) {

    for(int id = 0; id < count; id++) {
        if(buffer_list[port][id] != NULL) {
            free(buffer_list[port][id]);
        }
    }
}

int isStackEmpty(int port) {
    return (stack[port]->top == -1);
}

int isStackFull(int port) {
    return (stack[port]->top == NUMBER_OF_POOL_BUFFER - 1);
}

int buffer_pool_free(int port, BUF_POINTER element) {

    pthread_mutex_lock(&stack[port]->mutex);

    if (isStackFull(port)) {
        debug_printf("Stack[%d] is full. Cannot buffer_pool_free.\n", port);
        pthread_mutex_unlock(&stack[port]->mutex);
        return -1;
    }

    stack[port]->top++;
    stack[port]->elements[stack[port]->top] = element;

    pthread_mutex_unlock(&stack[port]->mutex);

    return 0;
}

BUF_POINTER buffer_pool_alloc(int port) {
    pthread_mutex_lock(&stack[port]->mutex);

    if (isStackEmpty(port)) {
        debug_printf("Stack[%d] is empty. Cannot buffer_pool_alloc.\n", port);
        pthread_mutex_unlock(&stack[port]->mutex);
        return EMPTY_ELEMENT;
    }

    BUF_POINTER poppedElement = stack[port]->elements[stack[port]->top];
    stack[port]->top--;

    pthread_mutex_unlock(&stack[port]->mutex);

    return poppedElement;
}

void quickSort(BUF_POINTER arr[], int left, int right) {

    int i = left, j = right;
    BUF_POINTER pivot = arr[(left + right) / 2];

    while (i <= j) {
        while (arr[i] < pivot) {
            i++;
        }
        while (arr[j] > pivot) {
            j--;
        }
        if (i <= j) {
            BUF_POINTER temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
            i++;
            j--;
        }
    }

    if (left < j) {
        quickSort(arr, left, j);
    }
    if (i < right) {
        quickSort(arr, i, right);
    }
}

int initialize_buffer_allocation() {

    char *buffer;
    int id;

    for(int port = 0; port < NUMBER_OF_ETH_PORT; port++) {
        stack[port] = &xdma_buffer_pool_stack[port];

        stack[port]->top = -1;
        pthread_mutex_init(&stack[port]->mutex, NULL);

        for(id = 0; id < NUMBER_OF_BUFFER; id++) {
            buffer = NULL;

            if(posix_memalign((void **)&buffer, BUFFER_ALIGNMENT /*alignment */, MAX_BUFFER_LENGTH + BUFFER_ALIGNMENT)) {
                fprintf(stderr, "OOM %u.\n", MAX_BUFFER_LENGTH + BUFFER_ALIGNMENT);
                relese_buffers(port, id);
                pthread_mutex_destroy(&stack[port]->mutex);
                for(int port_id = 0; port_id < port; port_id++) {
                    relese_buffers(port_id, NUMBER_OF_BUFFER);
                    pthread_mutex_destroy(&stack[port_id]->mutex);
                }
                return -1;
            }
            buffer_list[port][id] = buffer;
        }

        for (id = 0; id < NUMBER_OF_BUFFER; id++) {
            debug_printf("%p\n", buffer_list[port][id]);
        }
        debug_printf("\n");

        quickSort(&buffer_list[port][0], 0, (NUMBER_OF_BUFFER - 1));

        for (id = 0; id < NUMBER_OF_BUFFER; id++) {
            debug_printf("%p\n", buffer_list[port][id]);
        }
        debug_printf("\n");

        for(id = 0; id < NUMBER_OF_BUFFER; id++) {
            if(buffer_pool_free(port, buffer_list[port][id])) {
                relese_buffers(port, id + 1);
                pthread_mutex_destroy(&stack[port]->mutex);
                for(int port_id = 0; port_id < port; port_id++) {
                    relese_buffers(port_id, NUMBER_OF_BUFFER);
                    pthread_mutex_destroy(&stack[port_id]->mutex);
                }
                return -1;
            }
        }

        printf("  stack[%d]->elements[%4d]: %p\n", port, 0, stack[port]->elements[0]);
        printf("  stack[%d]->elements[%4d]: %p\n", port, NUMBER_OF_BUFFER-1, stack[port]->elements[NUMBER_OF_BUFFER-1]);
    }

    printf("Successfully allocated buffers(%u)\n", NUMBER_OF_BUFFER * NUMBER_OF_ETH_PORT);

    return 0;
}

void buffer_release() {

    for(int port = 0; port < NUMBER_OF_ETH_PORT; port++) {
        relese_buffers(port, NUMBER_OF_BUFFER);
        pthread_mutex_destroy(&stack[port]->mutex);
    }

    printf("Successfully release buffers(%u)\n", NUMBER_OF_BUFFER * NUMBER_OF_ETH_PORT);
}

