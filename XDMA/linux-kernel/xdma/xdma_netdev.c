#include "xdma_netdev.h"
#include "xdma_mod.h"
#include "cdev_sgdma.h"
#include "libxdma.h"

static void tx_desc_set(struct xdma_desc *desc, dma_addr_t addr, u32 len)
{
        u32 control_field;
        u32 control;

        desc->control = cpu_to_le32(DESC_MAGIC);
        control_field = XDMA_DESC_STOPPED;
        control_field |= XDMA_DESC_EOP; 
        control_field |= XDMA_DESC_COMPLETED;
        control = le32_to_cpu(desc->control & ~(LS_BYTE_MASK));
        control |= control_field;
        desc->control = cpu_to_le32(control);

        desc->src_addr_lo = cpu_to_le32(PCI_DMA_L(addr));
        desc->src_addr_hi = cpu_to_le32(PCI_DMA_H(addr));
        desc->bytes = cpu_to_le32(len);
}

void rx_desc_set(struct xdma_desc *desc, dma_addr_t addr, u32 len)
{
        u32 control_field;
        u32 control;

        desc->control = cpu_to_le32(DESC_MAGIC);
        control_field = XDMA_DESC_STOPPED;
        control_field |= XDMA_DESC_EOP; 
        control_field |= XDMA_DESC_COMPLETED;
        control = le32_to_cpu(desc->control & ~(LS_BYTE_MASK));
        control |= control_field;
        desc->control = cpu_to_le32(control);

        desc->dst_addr_lo = cpu_to_le32(PCI_DMA_L(addr));
        desc->dst_addr_hi = cpu_to_le32(PCI_DMA_H(addr));
        desc->bytes = cpu_to_le32(len);
}

int xdma_netdev_open(struct net_device *ndev)
{
        struct xdma_private *priv = netdev_priv(ndev);
        u32 w;

        netif_carrier_on(ndev);
        netif_start_queue(ndev);
        /* Set the RX descriptor */
        rx_desc_set(priv->rx_desc, priv->dma_addr, XDMA_BUFFER_SIZE);
        ioread32(&priv->rx_engine->regs->status_rc);

        /* RX start */
        w = cpu_to_le32(PCI_DMA_L(priv->rx_bus_addr));
        iowrite32(w, &priv->rx_engine->sgdma_regs->first_desc_lo);

        w = cpu_to_le32(PCI_DMA_H(priv->rx_bus_addr));
        iowrite32(w, &priv->rx_engine->sgdma_regs->first_desc_hi);
        iowrite32(DMA_ENGINE_START, &priv->rx_engine->regs->control);
        ioread32(&priv->rx_engine->regs->status);
        return 0;
}

int xdma_netdev_close(struct net_device *ndev)
{
        struct xdma_private *priv = netdev_priv(ndev);

        netif_stop_queue(ndev);
        netif_carrier_off(ndev);
        ioread32(&priv->rx_engine->regs->status_rc);
        return 0;
}


int check_desc1_status(struct xdma_private *priv, int index, dma_addr_t addr, u32 len, struct sk_buff *skb)
{
        unsigned long flag;
        struct xdma_desc *desc = priv->desc[index];

        spin_lock_irqsave(&priv->desc_lock[index], flag);
        /* Need to check if the descriptor is empty */
        if (desc->src_addr_lo == 0 && desc->src_addr_hi == 0) {
                tx_desc_set(desc, addr, len);
                priv->skb[index] = skb;
                spin_unlock_irqrestore(&priv->desc_lock[index], flag);
                return DESC_READY;
        }
        spin_unlock_irqrestore(&priv->desc_lock[index], flag);
        return DESC_BUSY;
}

int check_desc2_status(struct xdma_private *priv, int index, dma_addr_t addr, u32 len, int desc1_status, struct sk_buff *skb)
{
        unsigned long flag;
        struct xdma_desc *desc = priv->desc[index];

        spin_lock_irqsave(&priv->desc_lock[index], flag);

        /* Need to check if the descriptor is empty */
        if (desc->src_addr_lo == 0 && desc->src_addr_hi == 0) {
                if (desc1_status == DESC_READY) {
                        spin_unlock_irqrestore(&priv->desc_lock[index], flag);
                        return DESC_EMPTY;
                }
                tx_desc_set(desc, addr, len);
                priv->skb[index] = skb;
                spin_unlock_irqrestore(&priv->desc_lock[index], flag);
                return DESC_READY;
        }
        spin_unlock_irqrestore(&priv->desc_lock[index], flag);
        return DESC_BUSY;
}

netdev_tx_t xdma_netdev_start_xmit(struct sk_buff *skb,
                struct net_device *ndev)
{
        struct xdma_private *priv = netdev_priv(ndev);
        struct xdma_dev *xdev = priv->xdev;
        int padding = 0;
        unsigned long flag;
        int index;
        int desc1_status;
        int desc2_status;
        u32 w;
        dma_addr_t dma_addr;
        dma_addr_t bus_addr;

        /* Check desc count */
        spin_lock_irqsave(&priv->cnt_lock, flag);
        if (priv->count == 2) {
                spin_unlock_irqrestore(&priv->cnt_lock, flag);
                return NETDEV_TX_BUSY;
        }
        spin_unlock_irqrestore(&priv->cnt_lock, flag);

        padding = (skb->len < ETH_ZLEN) ? (ETH_ZLEN - skb->len) : 0;
        skb->len += padding;
        if (skb_padto(skb, skb->len)) {
                pr_err("skb_padto failed\n");
                dev_kfree_skb(skb);
                return NETDEV_TX_OK;
        }

        /* Jumbo frames not supported */
        if (skb->len > XDMA_BUFFER_SIZE) {
                pr_err("Jumbo frames not supported\n");
                dev_kfree_skb(skb);
                return NETDEV_TX_OK;
        }

        /* Add metadata to the skb */
        if (pskb_expand_head(skb, TX_METADATA_SIZE, 0, GFP_ATOMIC) != 0) {
                pr_err("pskb_expand_head failed\n");
                dev_kfree_skb(skb);
                return NETDEV_TX_OK;
        }
        skb_push(skb, TX_METADATA_SIZE);
        memset(skb->data, 0, TX_METADATA_SIZE);

        dma_addr = dma_map_single(&xdev->pdev->dev, skb->data, skb->len, DMA_TO_DEVICE);
        if (unlikely(dma_mapping_error(&xdev->pdev->dev, dma_addr))) {
                pr_err("dma_map_single failed\n");
                return NETDEV_TX_BUSY;
        }

        desc1_status = check_desc1_status(priv, 0, dma_addr, skb->len, skb);
        desc2_status = check_desc2_status(priv, 1, dma_addr, skb->len, desc1_status, skb);

        if (desc1_status == DESC_READY) {
                bus_addr = priv->bus_addr[0];
                index = 0;
        } else {
                bus_addr = priv->bus_addr[1];
                index = 1;
        }

        spin_lock_irqsave(&priv->cnt_lock, flag);
        priv->count++;
        /* If descs are full HW interrupt handler will send the other packet */
        if (priv->count == 2) {
                spin_unlock_irqrestore(&priv->cnt_lock, flag);
                return NETDEV_TX_OK;
        }
        spin_unlock_irqrestore(&priv->cnt_lock, flag);

        spin_lock_irqsave(&priv->tx_lock, flag);
        priv->last = index;

        w = cpu_to_le32(PCI_DMA_L(bus_addr));
        iowrite32(w, xdev->bar[1] + DESC_REG_LO);

        w = cpu_to_le32(PCI_DMA_H(bus_addr));
        iowrite32(w, xdev->bar[1] + DESC_REG_HI);
        
        iowrite32(DMA_ENGINE_START, &priv->tx_engine->regs->control);
        ioread32(&priv->tx_engine->regs->status);
        spin_unlock_irqrestore(&priv->tx_lock, flag);

        return NETDEV_TX_OK;
}
