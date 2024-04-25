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
        dma_addr_t dma_addr;
        u32 lo, hi;
        long flag;

        netif_carrier_on(ndev);
        netif_start_queue(ndev);

        /* Set the RX descriptor */
        dma_addr = dma_map_single(
                        &priv->xdev->pdev->dev,
                        priv->res,
                        sizeof(struct xdma_result),
                        DMA_FROM_DEVICE);
        priv->rx_desc->src_addr_lo = cpu_to_le32(PCI_DMA_L(dma_addr));
        priv->rx_desc->src_addr_hi = cpu_to_le32(PCI_DMA_H(dma_addr));
        rx_desc_set(priv->rx_desc, priv->rx_dma_addr, XDMA_BUFFER_SIZE);
        spin_lock_irqsave(&priv->rx_lock, flag);
        ioread32(&priv->rx_engine->regs->status_rc);

        /* RX start */
        lo = cpu_to_le32(PCI_DMA_L(priv->rx_bus_addr));
        iowrite32(lo, &priv->rx_engine->sgdma_regs->first_desc_lo);

        hi = cpu_to_le32(PCI_DMA_H(priv->rx_bus_addr));
        iowrite32(hi, &priv->rx_engine->sgdma_regs->first_desc_hi);

        iowrite32(DMA_ENGINE_START, &priv->rx_engine->regs->control);
        spin_unlock_irqrestore(&priv->rx_lock, flag);

        return 0;
}

int xdma_netdev_close(struct net_device *ndev)
{
        netif_stop_queue(ndev);
        pr_info("xdma_netdev_close\n");
        netif_carrier_off(ndev);
        pr_info("netif_carrier_off\n");
        return 0;
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
        netif_stop_queue(ndev);
        pr_err("xdma_netdev_start_xmit\n");
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
        priv->tx_dma_addr = dma_addr;
        priv->tx_skb = skb;
        tx_desc_set(priv->tx_desc, dma_addr, skb->len);

        w = cpu_to_le32(PCI_DMA_L(priv->tx_bus_addr));
        iowrite32(w, xdev->bar[1] + DESC_REG_LO);

        w = cpu_to_le32(PCI_DMA_H(priv->tx_bus_addr));
        iowrite32(w, xdev->bar[1] + DESC_REG_HI);
        iowrite32(0, xdev->bar[1] + DESC_REG_HI + 4);

        iowrite32(DMA_ENGINE_START, &priv->tx_engine->regs->control);
        return NETDEV_TX_OK;
}
