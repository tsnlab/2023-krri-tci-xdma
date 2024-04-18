/*
 * packet_handler.h
 *
 *  Created on: Apr 24, 2023
 *      Author: pooky
 */

#ifndef TSN_PACKET_HANDLER_H_
#define TSN_PACKET_HANDLER_H_

int32_t packet_handler_main(int32_t count);

int32_t transmit_arp_packet();

void buffer_pool_push(struct tsn_tx_buffer* buffer);


void init_stacks();

#endif /* TSN_PACKET_HANDLER_H_ */
