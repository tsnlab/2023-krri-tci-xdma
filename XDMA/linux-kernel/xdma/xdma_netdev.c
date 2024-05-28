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
        unsigned long flag;

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

#if DEBUG_ONE_QUEUE_TSN_
void dump_buffer(unsigned char* buffer, int len)
{
        int i = 0;
        pr_err("[Buffer]");
        pr_err("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
        buffer[i+0] & 0xFF, buffer[i+1] & 0xFF, buffer[i+2] & 0xFF, buffer[i+3] & 0xFF,
        buffer[i+4] & 0xFF, buffer[i+5] & 0xFF, buffer[i+6] & 0xFF, buffer[i+7] & 0xFF,
        buffer[i+8] & 0xFF, buffer[i+9] & 0xFF, buffer[i+10] & 0xFF, buffer[i+11] & 0xFF,
        buffer[i+11] & 0xFF, buffer[i+13] & 0xFF, buffer[i+14] & 0xFF, buffer[i+15] & 0xFF);

        i = 16;
        pr_err("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
        buffer[i+0] & 0xFF, buffer[i+1] & 0xFF, buffer[i+2] & 0xFF, buffer[i+3] & 0xFF,
        buffer[i+4] & 0xFF, buffer[i+5] & 0xFF, buffer[i+6] & 0xFF, buffer[i+7] & 0xFF,
        buffer[i+8] & 0xFF, buffer[i+9] & 0xFF, buffer[i+10] & 0xFF, buffer[i+11] & 0xFF,
        buffer[i+11] & 0xFF, buffer[i+13] & 0xFF, buffer[i+14] & 0xFF, buffer[i+15] & 0xFF);

        i = 32;
        pr_err("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
        buffer[i+0] & 0xFF, buffer[i+1] & 0xFF, buffer[i+2] & 0xFF, buffer[i+3] & 0xFF,
        buffer[i+4] & 0xFF, buffer[i+5] & 0xFF, buffer[i+6] & 0xFF, buffer[i+7] & 0xFF,
        buffer[i+8] & 0xFF, buffer[i+9] & 0xFF, buffer[i+10] & 0xFF, buffer[i+11] & 0xFF,
        buffer[i+11] & 0xFF, buffer[i+13] & 0xFF, buffer[i+14] & 0xFF, buffer[i+15] & 0xFF);

        pr_err("\n");
}
#endif

netdev_tx_t xdma_netdev_start_xmit(struct sk_buff *skb,
                struct net_device *ndev)
{
        struct xdma_private *priv = netdev_priv(ndev);
        struct xdma_dev *xdev = priv->xdev;
        int padding = 0;
        u32 w;
        u32 sys_count_low;
        u16 frame_length;
        dma_addr_t dma_addr;
        struct tx_metadata* tx_metadata;

        /* Check desc count */
        netif_stop_queue(ndev);
#if DEBUG_ONE_QUEUE_TSN_
        pr_err("xdma_netdev_start_xmit(skb->len : %d)\n", skb->len);
#endif
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

        /* Store packet length */
        frame_length = skb->len;

        /* Add metadata to the skb */
        if (pskb_expand_head(skb, TX_METADATA_SIZE, 0, GFP_ATOMIC) != 0) {
                pr_err("pskb_expand_head failed\n");
                dev_kfree_skb(skb);
                return NETDEV_TX_OK;
        }
        skb_push(skb, TX_METADATA_SIZE);
        memset(skb->data, 0, TX_METADATA_SIZE);

#if DEBUG_ONE_QUEUE_TSN_
        pr_err("skb->len : %d\n", skb->len);
#endif
        struct tx_buffer* tx_buffer = (struct tx_buffer*)skb->data;
        /* Fill in the metadata */
        tx_metadata = (struct tx_metadata*)&tx_buffer->metadata;
        tx_metadata->frame_length = frame_length;
        //tx_metadata->timestamp_id = 0;
        //tx_metadata->fail_policy = 0;

        /* Reads the lower 29 bits of the system count. */
        sys_count_low = (uint32_t)(ioread32(xdev->bar[0] + 0x0384) & 0x1FFFFFFF);

        /* Set the fromtick & to_tick values based on the lower 29 bits of the system count */
        tx_metadata->from.tick = (uint32_t)((sys_count_low + _DEFAULT_FROM_MARGIN_) & 0x1FFFFFFF);
        tx_metadata->to.tick = (uint32_t)((sys_count_low + _DEFAULT_TO_MARGIN_) & 0x1FFFFFFF);

#if DEBUG_ONE_QUEUE_TSN_
        pr_err("0x%08x  0x%08x  0x%08x  %4d  %1d",
                sys_count_low, tx_metadata->from.tick, tx_metadata->to.tick,
                tx_metadata->frame_length, tx_metadata->fail_policy);
        dump_buffer((unsigned char*)tx_metadata, (int)(sizeof(struct tx_metadata) + skb->len));
#endif

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
