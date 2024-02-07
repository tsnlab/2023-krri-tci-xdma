#include "xdma_netdev.h"
#include "xdma_mod.h"
#include "cdev_sgdma.h"
#include "libxdma.h"

#ifdef XDMA_DEBUG
static void dump_packet(unsigned char *buf, int len)
{
        int i;
        for (i = 0; i < len; i++) {
                if (i % 16 == 0) pr_err("\n");
                pr_err("%02x ", buf[i]);
        }
        pr_err("\n");
}
#endif

static int check_transfer_align_packet(struct xdma_engine *engine,
        const char __kernel *buf, size_t count, loff_t pos, int sync)
{
        if (!engine) {
                pr_err("Invalid DMA engine\n");
                return -EINVAL;
        }
        /* AXI ST or AXI MM non-incremental addressing mode? */
        if (engine->non_incr_addr) {
                int buf_lsb = (int)((uintptr_t)buf) & (engine->addr_align - 1);
                size_t len_lsb = count & ((size_t)engine->len_granularity - 1);
                int pos_lsb = (int)pos & (engine->addr_align - 1);

                dbg_tfr("AXI ST or MM non-incremental\n");
                dbg_tfr("buf_lsb = %d, pos_lsb = %d, len_lsb = %ld\n", buf_lsb,
                        pos_lsb, len_lsb);

                if (buf_lsb != 0) {
                        dbg_tfr("FAIL: non-aligned buffer address %p\n", buf);
                        return -EINVAL;
                }

                if ((pos_lsb != 0) && (sync)) {
                        dbg_tfr("FAIL: non-aligned AXI MM FPGA addr 0x%llx\n",
                                (unsigned long long)pos);
                        return -EINVAL;
                }

                if (len_lsb != 0) {
                        dbg_tfr(
                                "FAIL: len %d is not a multiple of %d\n",
                                (int)count,
                                (int)engine->len_granularity);
                        return -EINVAL;
                }
                /* AXI MM incremental addressing mode */
        } else {
                int buf_lsb = (int)((uintptr_t)buf) & (engine->addr_align - 1);
                int pos_lsb = (int)pos & (engine->addr_align - 1);

                if (buf_lsb != pos_lsb) {
                        dbg_tfr("FAIL: Misalignment error\n");
                        dbg_tfr("host addr %p, FPGA addr 0x%llx\n", buf, pos);
                        return -EINVAL;
                }
        }
        return 0;
}

static void char_sgdma_unmap_kernel_buf(struct xdma_io_cb *cb, bool write)
{
        int i;

        sg_free_table(&cb->sgt);
        if (!cb->pages || !cb->pages_nr) {
                return;
        }

        for (i = 0; i < cb->pages_nr; i++) {
                if (cb->pages[i]) {
                        if (!write) {
                                set_page_dirty_lock(cb->pages[i]);
                        }
                        put_page(cb->pages[i]);
                } else
                        break;
        }

        if (i != cb->pages_nr)
                pr_info("sgl pages %d/%u.\n", i, cb->pages_nr);

        kfree(cb->pages);
        cb->pages = NULL;
}

static int char_sgdma_map_kernel_buf_to_sgl(struct xdma_io_cb *cb, bool write)
{
        struct sg_table *sgt = &cb->sgt;
        unsigned long len = cb->len;
        void __kernel *buf = cb->buf;
        struct scatterlist *sg;
        unsigned int pages_nr = (
            ((unsigned long)buf + len + PAGE_SIZE - 1)
            - ((unsigned long)buf & PAGE_MASK)
        ) >> PAGE_SHIFT;
        int i;
        int rv;
        struct kvec kv;
        memset(&kv, 0, sizeof(kv));
        kv.iov_base = (void *)cb->buf;
        kv.iov_len = PAGE_SIZE;
        if (pages_nr == 0)
                return -EINVAL;

        if (sg_alloc_table(sgt, pages_nr, GFP_KERNEL)) {
                pr_err("sgl OOM.\n");
                return -ENOMEM;
        }

        cb->pages = kcalloc(pages_nr, sizeof(struct page *), GFP_KERNEL);
        if (!cb->pages) {
                pr_err("pages OOM.\n");
                rv = -ENOMEM;
                goto err_out;
        }
        rv = get_kernel_pages(&kv, pages_nr, 1, cb->pages);
        /* No pages were pinned */
        if (rv < 0) {
                pr_err("unable to pin down %u kernel pages, %d.\n",
                        pages_nr, rv);
                goto err_out;
        }
        /* Less pages pinned than wanted */
        if (rv != pages_nr) {
                pr_err("unable to pin down all %u kernel pages, %d.\n",
                        pages_nr, rv);
                cb->pages_nr = rv;
                rv = -EFAULT;
                goto err_out;
        }

        /*
         * Check for duplicate pages
         * To avoid segmenation fault i start from 1
         * Because i compare the current page with the previous one
         */
        for (i = 1; i < pages_nr; i++) {
                if (cb->pages[i - 1] == cb->pages[i]) {
                        pr_err("duplicate pages, %d, %d.\n",
                                i - 1, i);
                        rv = -EFAULT;
                        cb->pages_nr = pages_nr;
                        goto err_out;
                }
        }

        sg = sgt->sgl;
        for (i = 0; i < pages_nr; i++, sg = sg_next(sg)) {
                if (!sg)
                        break;
                unsigned int offset = offset_in_page(buf);
                unsigned int nbytes =
                        min_t(unsigned int, PAGE_SIZE - offset, len);

                flush_dcache_page(cb->pages[i]);
                sg_set_page(sg, cb->pages[i], nbytes, offset);

                buf += nbytes;
                len -= nbytes;
        }

        if (len) {
                pr_err("Invalid kernel buffer length. Cannot map to sgl\n");
                return -EINVAL;
        }
        cb->pages_nr = pages_nr;
        return 0;

err_out:
        char_sgdma_unmap_kernel_buf(cb, write);

        return rv;
}

int xdma_rx_handler(struct net_device *ndev)
{
        int ret;
        struct xdma_private *priv = netdev_priv(ndev);
        struct xdma_dev *xdev = priv->xdev;
        struct xdma_engine *engine = priv->rx_engine;
        struct xdma_io_cb cb;

        ret = check_transfer_align_packet(engine, priv->rx_buffer,
                        XDMA_BUFFER_SIZE, 0, 1);
        if (ret) {
                pr_err("Invalid transfer alignment\n");
                return ret;
        }

        memset(&cb, 0, sizeof(cb));
        memset(priv->rx_buffer, 0, XDMA_BUFFER_SIZE);

        cb.buf = priv->rx_buffer;
	/*
	 * FIXME: Currently driver doesn't know size of packets in H/W
	 * so we set cb.len to XDMA_BUFFER_SIZE and this should be changed
	 */
        cb.len = XDMA_BUFFER_SIZE;
        cb.ep_addr = 0;
        cb.write = 0;

        /* Map kernel buffer to SGL */
        ret = char_sgdma_map_kernel_buf_to_sgl(&cb, 0);
        if (ret < 0) {
                pr_err("Failed to map kernel buffer to SGL\n");
                return ret;
        }

        /*
         * Set desc and transfer to board
         * The board set the packet to the kernel buffer
         */
        ret = xdma_xfer_submit(xdev,
                        engine->channel,
                        0,
                        0,
                        &cb.sgt,
                        0,
                        1000);

        char_sgdma_unmap_kernel_buf(&cb, 0);
        return ret;
}
int xdma_tx_handler(struct net_device *ndev)
{
        int ret;
        struct xdma_private *priv = netdev_priv(ndev);
        struct xdma_dev *xdev = priv->xdev;
        struct xdma_engine *engine = priv->tx_engine;
        struct xdma_io_cb cb;

        ret = check_transfer_align_packet(engine, priv->skb->data,
                        priv->skb->len, 0, 1);
        if (ret) {
                pr_err("Invalid transfer alignment\n");
                return ret;
        }

        memset(&cb, 0, sizeof(cb));
        memset(priv->tx_buffer, 0, XDMA_BUFFER_SIZE);
        memcpy(priv->tx_buffer + 8, priv->skb->data, priv->skb->len);

        cb.buf = priv->tx_buffer;
        cb.len = priv->skb->len + TX_METADATA_SIZE;
        cb.ep_addr = 0;
        cb.write = 1;

        /* Map kernel buffer to SGL */
        ret = char_sgdma_map_kernel_buf_to_sgl(&cb, 1);
        if (ret < 0) {
                pr_err("Failed to map kernel buffer to SGL\n");
                return ret;
        }

        /*
         * Set desc and transfer to board
         * The board send the packet to peer
         */
        ret = xdma_xfer_submit(
                xdev,
                engine->channel,
                1,
                0,
                &cb.sgt,
                0,
                1000);
        char_sgdma_unmap_kernel_buf(&cb, 1);
        dev_kfree_skb(priv->skb);
        netif_wake_queue(ndev);
        return ret;
}

int xdma_netdev_open(struct net_device *ndev)
{
        netif_start_queue(ndev);
        netif_carrier_on(ndev);
        return 0;
}

int xdma_netdev_close(struct net_device *ndev)
{
        netif_stop_queue(ndev);
        netif_carrier_off(ndev);
        return 0;
}

static void desc_set(struct xdma_desc *desc, dma_addr_t addr, u32 len)
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

netdev_tx_t xdma_netdev_start_xmit(struct sk_buff *skb,
                struct net_device *ndev)
{
        struct xdma_private *priv = netdev_priv(ndev);
        struct pci_dev *pdev = priv->pdev;
        struct xdma_dev *xdev = priv->xdev;
        int padding = 0;
        int flag;
        int index;
        u32 control;
        u32 w;
        dma_addr_t dma_addr;

        /* Stop the queue */
        netif_stop_queue(ndev);
        padding = (skb->len < ETH_ZLEN) ? (ETH_ZLEN - skb->len) : 0;
        skb->len += padding;
        if (skb_padto(skb, skb->len)) {
                pr_err("skb_padto failed\n");
                return NETDEV_TX_OK;
        }

        /* Jumbo frames not supported */
        if (skb->len > XDMA_BUFFER_SIZE) {
                pr_err("Jumbo frames not supported\n");
                dev_kfree_skb(skb);
                return NETDEV_TX_OK;
        }

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

        priv->dma_addr = dma_addr;
        priv->skb = skb;

        desc_set(priv->desc, dma_addr, skb->len);
        
        /* Write bus address of descriptor to register */
        w = cpu_to_le32(PCI_DMA_L(priv->bus_addr));
        iowrite32(w, xdev->bar[1] + DESC_REG_LO);

        w = cpu_to_le32(PCI_DMA_H(priv->bus_addr));
        iowrite32(w, xdev->bar[1] + DESC_REG_HI);
        iowrite32(0, xdev->bar[1] + DESC_REG_HI + 4);

        w = (u32)XDMA_CTRL_RUN_STOP;
	w |= (u32)XDMA_CTRL_IE_READ_ERROR;
	w |= (u32)XDMA_CTRL_IE_DESC_ERROR;
	w |= (u32)XDMA_CTRL_IE_DESC_ALIGN_MISMATCH;
	w |= (u32)XDMA_CTRL_IE_MAGIC_STOPPED;
	w |= (u32)XDMA_CTRL_IE_DESC_STOPPED;
	w |= (u32)XDMA_CTRL_IE_DESC_COMPLETED;

        iowrite32(w, &priv->tx_engine->regs->control);
        return NETDEV_TX_OK;
}
