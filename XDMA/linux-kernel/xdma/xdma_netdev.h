#ifndef XDMA_NETDEV_H
#define XDMA_NETDEV_H
#endif

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>

#include "xdma_mod.h"

#define RX_METADATA_SIZE 16
#define TX_METADATA_SIZE 8

#define DESC_REG_LO SGDMA_OFFSET_FROM_CHANNEL + 0x80
#define DESC_REG_HI SGDMA_OFFSET_FROM_CHANNEL + 0x84

struct xdma_private {
        struct pci_dev *pdev;
        struct net_device *ndev;
        struct xdma_dev *xdev;
        struct xdma_engine *tx_engine;
        struct xdma_engine *rx_engine;
        struct xdma_desc *desc;
        struct sk_buff *skb;
        struct work_struct tx_work;
        struct work_struct rx_work;
        struct mutex lock;
        dma_addr_t dma_addr;
        dma_addr_t bus_addr;
        u8 *tx_buffer;
        u8 *rx_buffer;
        spinlock_t skb_lock;
};

/*
 * xdma_tx_handler - Transmit packet
 * @ndev: Pointer to the network device
 */
int xdma_tx_handler(struct net_device *ndev);

/*
 * xdma_rx_handler - Receive packet
 * @ndev: Pointer to the network device
 */
int xdma_rx_handler(struct net_device *ndev);

/*
 * xdma_netdev_open - Open the network device
 * @netdev: Pointer to the network device
 */
int xdma_netdev_open(struct net_device *netdev);

/*
 * xdma_netdev_close - Close the network device
 * @netdev: Pointer to the network device
 */
int xdma_netdev_close(struct net_device *netdev);

/*
 * xdma_netdev_start_xmit - Tx handler
 * If user transmits a packet, this function is called
 * @skb: Pointer to the socket buffer
 * @netdev: Pointer to the network device
 */
netdev_tx_t xdma_netdev_start_xmit(struct sk_buff *skb,
                                   struct net_device *netdev);
