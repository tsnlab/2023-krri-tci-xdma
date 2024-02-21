#ifndef XDMA_NETDEV_H
#define XDMA_NETDEV_H
#endif

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>

#include "xdma_mod.h"

#define RX_METADATA_SIZE 16
#define TX_METADATA_SIZE 8

#define DESC_REG_LO SGDMA_OFFSET_FROM_CHANNEL + 0x80
#define DESC_REG_HI SGDMA_OFFSET_FROM_CHANNEL + 0x84

#define DESC_REG_LO_RX SGDMA_OFFSET_FROM_CHANNEL_RX + 0x80
#define DESC_REG_HI_RX SGDMA_OFFSET_FROM_CHANNEL_RX + 0x84

#define DMA_ENGINE_START 16268831
#define DMA_ENGINE_STOP 16268830

#define DESC_EMPTY 0
#define DESC_READY 1
#define DESC_BUSY 2

struct xdma_private {
        struct pci_dev *pdev;
        struct net_device *ndev;
        struct xdma_dev *xdev;
        struct xdma_engine *tx_engine;
        struct xdma_engine *rx_engine;
        struct xdma_desc *rx_desc;
        struct xdma_desc *desc[2];
        dma_addr_t bus_addr[2];
        dma_addr_t rx_bus_addr;
        struct sk_buff *rx_skb;
        struct sk_buff *skb[2];
        struct work_struct tx_work;
        struct work_struct rx_work;
        dma_addr_t dma_addr;
        u8 *tx_buffer;
        u8 *rx_buffer;
        spinlock_t tx_lock;
        spinlock_t desc_lock[2];
        spinlock_t cnt_lock;
        spinlock_t rx_lock;
        int last;
        int count;
        int irq;
        int rx_count;
};

void rx_desc_set(struct xdma_desc *desc, dma_addr_t addr, u32 len);

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
