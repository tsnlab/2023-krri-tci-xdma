/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 * Copyright (c) 2016-present,  Xilinx, Inc. All rights reserved.
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/aer.h>
/* include early, to verify it depends only on the headers above */
#include "libxdma_api.h"
#include "libxdma.h"
#include "xdma_mod.h"
#include "xdma_cdev.h"
#include "version.h"
#include "xdma_netdev.h"

#define DRV_MODULE_NAME		"xdma"
#define DRV_MODULE_DESC		"Xilinx XDMA Reference Driver"

static char version[] =
	DRV_MODULE_DESC " " DRV_MODULE_NAME " v" DRV_MODULE_VERSION "\n";

MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION(DRV_MODULE_DESC);
MODULE_VERSION(DRV_MODULE_VERSION);
MODULE_LICENSE("Dual BSD/GPL");

/* SECTION: Module global variables */
static int xpdev_cnt;

static const struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(0x10ee, 0x9048), },
	{ PCI_DEVICE(0x10ee, 0x9044), },
	{ PCI_DEVICE(0x10ee, 0x9042), },
	{ PCI_DEVICE(0x10ee, 0x9041), },
	{ PCI_DEVICE(0x10ee, 0x903f), },
	{ PCI_DEVICE(0x10ee, 0x9038), },
	{ PCI_DEVICE(0x10ee, 0x9028), },
	{ PCI_DEVICE(0x10ee, 0x9018), },
	{ PCI_DEVICE(0x10ee, 0x9034), },
	{ PCI_DEVICE(0x10ee, 0x9024), },
	{ PCI_DEVICE(0x10ee, 0x9014), },
	{ PCI_DEVICE(0x10ee, 0x9032), },
	{ PCI_DEVICE(0x10ee, 0x9022), },
	{ PCI_DEVICE(0x10ee, 0x9012), },
	{ PCI_DEVICE(0x10ee, 0x9031), },
	{ PCI_DEVICE(0x10ee, 0x9021), },
	{ PCI_DEVICE(0x10ee, 0x9011), },

	{ PCI_DEVICE(0x10ee, 0x8011), },
	{ PCI_DEVICE(0x10ee, 0x8012), },
	{ PCI_DEVICE(0x10ee, 0x8014), },
	{ PCI_DEVICE(0x10ee, 0x8018), },
	{ PCI_DEVICE(0x10ee, 0x8021), },
	{ PCI_DEVICE(0x10ee, 0x8022), },
	{ PCI_DEVICE(0x10ee, 0x8024), },
	{ PCI_DEVICE(0x10ee, 0x8028), },
	{ PCI_DEVICE(0x10ee, 0x8031), },
	{ PCI_DEVICE(0x10ee, 0x8032), },
	{ PCI_DEVICE(0x10ee, 0x8034), },
	{ PCI_DEVICE(0x10ee, 0x8038), },

	{ PCI_DEVICE(0x10ee, 0x7011), },
	{ PCI_DEVICE(0x10ee, 0x7012), },
	{ PCI_DEVICE(0x10ee, 0x7014), },
	{ PCI_DEVICE(0x10ee, 0x7018), },
	{ PCI_DEVICE(0x10ee, 0x7021), },
	{ PCI_DEVICE(0x10ee, 0x7022), },
	{ PCI_DEVICE(0x10ee, 0x7024), },
	{ PCI_DEVICE(0x10ee, 0x7028), },
	{ PCI_DEVICE(0x10ee, 0x7031), },
	{ PCI_DEVICE(0x10ee, 0x7032), },
	{ PCI_DEVICE(0x10ee, 0x7034), },
	{ PCI_DEVICE(0x10ee, 0x7038), },

	{ PCI_DEVICE(0x10ee, 0x6828), },
	{ PCI_DEVICE(0x10ee, 0x6830), },
	{ PCI_DEVICE(0x10ee, 0x6928), },
	{ PCI_DEVICE(0x10ee, 0x6930), },
	{ PCI_DEVICE(0x10ee, 0x6A28), },
	{ PCI_DEVICE(0x10ee, 0x6A30), },
	{ PCI_DEVICE(0x10ee, 0x6D30), },

	{ PCI_DEVICE(0x10ee, 0x4808), },
	{ PCI_DEVICE(0x10ee, 0x4828), },
	{ PCI_DEVICE(0x10ee, 0x4908), },
	{ PCI_DEVICE(0x10ee, 0x4A28), },
	{ PCI_DEVICE(0x10ee, 0x4B28), },

	{ PCI_DEVICE(0x10ee, 0x2808), },

#ifdef INTERNAL_TESTING
	{ PCI_DEVICE(0x1d0f, 0x1042), 0},
#endif
	/* aws */
	{ PCI_DEVICE(0x1d0f, 0xf000), },
	{ PCI_DEVICE(0x1d0f, 0xf001), },

	{0,}
};
MODULE_DEVICE_TABLE(pci, pci_ids);

static void xpdev_free(struct xdma_pci_dev *xpdev)
{
	struct xdma_dev *xdev = xpdev->xdev;

	pr_info("xpdev 0x%p, destroy_interfaces, xdev 0x%p.\n", xpdev, xdev);
	xpdev_destroy_interfaces(xpdev);
	xpdev->xdev = NULL;
	pr_info("xpdev 0x%p, xdev 0x%p xdma_device_close.\n", xpdev, xdev);
	xdma_device_close(xpdev->pdev, xdev);
	xpdev_cnt--;

	kfree(xpdev);
}

static struct xdma_pci_dev *xpdev_alloc(struct pci_dev *pdev)
{
	struct xdma_pci_dev *xpdev = kmalloc(sizeof(*xpdev), GFP_KERNEL);

	if (!xpdev)
		return NULL;
	memset(xpdev, 0, sizeof(*xpdev));

	xpdev->magic = MAGIC_DEVICE;
	xpdev->pdev = pdev;
	xpdev->user_max = MAX_USER_IRQ;
	xpdev->h2c_channel_max = XDMA_CHANNEL_NUM_MAX;
	xpdev->c2h_channel_max = XDMA_CHANNEL_NUM_MAX;

	xpdev_cnt++;
	return xpdev;
}

/* TODO: Add tc function */
static const struct net_device_ops xdma_netdev_ops = {
	.ndo_open = xdma_netdev_open,
	.ndo_stop = xdma_netdev_close,
	.ndo_start_xmit = xdma_netdev_start_xmit,
};

static void tx_work_handler(struct work_struct *work)
{
	struct xdma_private *priv;

	priv = container_of(work, struct xdma_private, tx_work);
	xdma_tx_handler(priv->ndev);
}

static void rx_work_handler(struct work_struct *work)
{
	struct xdma_private *priv;

	priv = container_of(work, struct xdma_private, rx_work);
	xdma_rx_handler(priv->ndev);
}

static int rx_thread(void *data)
{
	struct xdma_private *priv = data;

	while (kthread_should_stop() == 0) {
		/* TODO: Check if the board got packets */
		schedule_work(&priv->rx_work);
		msleep(100);
	}
	return 0;
}

static struct task_struct *task;

static int probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rv = 0;
	struct xdma_pci_dev *xpdev = NULL;
	struct xdma_dev *xdev;
	void *hndl;
	struct net_device *ndev;
	struct xdma_private *priv;

	xpdev = xpdev_alloc(pdev);
	if (!xpdev)
		return -ENOMEM;

	hndl = xdma_device_open(DRV_MODULE_NAME, pdev, &xpdev->user_max,
			&xpdev->h2c_channel_max, &xpdev->c2h_channel_max);
	if (!hndl) {
		rv = -EINVAL;
		goto err_out;
	}

	if (xpdev->user_max > MAX_USER_IRQ) {
		pr_err("Maximum users limit reached\n");
		rv = -EINVAL;
		goto err_out;
	}

	if (xpdev->h2c_channel_max > XDMA_CHANNEL_NUM_MAX) {
		pr_err("Maximun H2C channel limit reached\n");
		rv = -EINVAL;
		goto err_out;
	}

	if (xpdev->c2h_channel_max > XDMA_CHANNEL_NUM_MAX) {
		pr_err("Maximun C2H channel limit reached\n");
		rv = -EINVAL;
		goto err_out;
	}

	if (!xpdev->h2c_channel_max && !xpdev->c2h_channel_max)
		pr_warn("NO engine found!\n");

	if (xpdev->user_max) {
		u32 mask = (1 << (xpdev->user_max + 1)) - 1;

		rv = xdma_user_isr_enable(hndl, mask);
		if (rv)
			goto err_out;
	}
	/* make sure no duplicate */
	xdev = xdev_find_by_pdev(pdev);
	if (!xdev) {
		pr_warn("NO xdev found!\n");
		rv =  -EINVAL;
		goto err_out;
	}

	if (hndl != xdev) {
		pr_err("xdev handle mismatch\n");
		rv =  -EINVAL;
		goto err_out;
	}

	pr_info("%s xdma%d, pdev 0x%p, xdev 0x%p, 0x%p, usr %d, ch %d,%d.\n",
		dev_name(&pdev->dev), xdev->idx, pdev, xpdev, xdev,
		xpdev->user_max, xpdev->h2c_channel_max,
		xpdev->c2h_channel_max);

	xpdev->xdev = hndl;

	rv = xpdev_create_interfaces(xpdev);
	if (rv)
		goto err_out;
	dev_set_drvdata(&pdev->dev, xpdev);

	/* Set the TSN register to 0x1 */
        iowrite32(0x1, xdev->bar[0] + 0x0008);
        iowrite32(0x800f0000, xdev->bar[0] + 0x610);
        iowrite32(0x10, xdev->bar[0] + 0x620);



	/* Allocate the network device */
	ndev = alloc_etherdev(sizeof(struct xdma_private));
	if (!ndev) {
		pr_err("alloc_etherdev failed\n");
		rv = -ENOMEM;
		goto err_out;
	}

	xdev->ndev = ndev;
	xpdev->ndev = ndev;

	/* Set up the network interface */
	ndev->netdev_ops = &xdma_netdev_ops;
	SET_NETDEV_DEV(ndev, &pdev->dev);
	priv = netdev_priv(ndev);
	memset(priv, 0, sizeof(struct xdma_private));
	priv->pdev = pdev;
	priv->ndev = ndev;
	priv->xdev = xpdev->xdev;
	priv->rx_engine = &xdev->engine_c2h[0];
	priv->tx_engine = &xdev->engine_h2c[0];
        priv->desc = dma_alloc_coherent(&pdev->dev, sizeof(struct xdma_desc),
                                        &priv->bus_addr, GFP_KERNEL);
	
	//mutex_init(&priv->lock);
	//spin_lock_init(&priv->skb_lock);

	memcpy(ndev->dev_addr, "\xca\x11\x22\x3a\x44\x55", ETH_ALEN);

	INIT_WORK(&priv->tx_work, tx_work_handler);
	INIT_WORK(&priv->rx_work, rx_work_handler);

	priv->tx_buffer = kmalloc(XDMA_BUFFER_SIZE, GFP_KERNEL);
	if (!priv->tx_buffer) {
		pr_err("kmalloc failed\n");
		free_netdev(ndev);
		rv = -ENOMEM;
		goto err_out;
	}

	priv->rx_buffer = kmalloc(XDMA_BUFFER_SIZE, GFP_KERNEL);
	if (!priv->rx_buffer) {
		pr_err("kmalloc failed\n");
		kfree(priv->tx_buffer);
		free_netdev(ndev);
		rv = -ENOMEM;
		goto err_out;
	}

	rv = register_netdev(ndev);
	if (rv < 0) {
		free_netdev(ndev);
		kfree(priv->tx_buffer);
		kfree(priv->rx_buffer);
		pr_err("register_netdev failed\n");
		goto err_out;
	}

	/* Polling thread */
	task = kthread_run(rx_thread, priv, "rx_thread");
	if (IS_ERR(task)) {
		pr_err("kthread_run failed\n");
		unregister_netdev(ndev);
		free_netdev(ndev);
		kfree(priv->tx_buffer);
		kfree(priv->rx_buffer);
		rv = -ENOMEM;
		goto err_out;
	}

	netif_carrier_off(ndev);
	netif_stop_queue(ndev);
	return 0;

err_out:
	pr_err("pdev 0x%p, err %d.\n", pdev, rv);
	xpdev_free(xpdev);
	return rv;
}

static void remove_one(struct pci_dev *pdev)
{
	struct xdma_pci_dev *xpdev;
	struct net_device *ndev;
	struct xdma_private *priv;

	if (!pdev)
		return;

	xpdev = dev_get_drvdata(&pdev->dev);
	if (!xpdev)
		return;

	pr_info("pdev 0x%p, xdev 0x%p, 0x%p.\n",
		pdev, xpdev, xpdev->xdev);

	/* Wait for the thread to stop */
	kthread_stop(task);

	ndev = xpdev->ndev;
	priv = netdev_priv(ndev);

	/* Wait for the work to stop */
	cancel_work_sync(&priv->tx_work);
	cancel_work_sync(&priv->rx_work);

	/* Free the network device */
        dma_free_coherent(&pdev->dev, sizeof(struct xdma_desc), priv->desc, priv->bus_addr);
	kfree(priv->tx_buffer);
	kfree(priv->rx_buffer);
	unregister_netdev(ndev);
	free_netdev(ndev);

	xpdev_free(xpdev);
	dev_set_drvdata(&pdev->dev, NULL);
}

static pci_ers_result_t xdma_error_detected(struct pci_dev *pdev,
					pci_channel_state_t state)
{
	struct xdma_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	switch (state) {
	case pci_channel_io_normal:
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		pr_warn("dev 0x%p,0x%p, frozen state error, reset controller\n",
			pdev, xpdev);
		xdma_device_offline(pdev, xpdev->xdev);
		pci_disable_device(pdev);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		pr_warn("dev 0x%p,0x%p, failure state error, req. disconnect\n",
			pdev, xpdev);
		return PCI_ERS_RESULT_DISCONNECT;
	}
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t xdma_slot_reset(struct pci_dev *pdev)
{
	struct xdma_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	pr_info("0x%p restart after slot reset\n", xpdev);
	if (pci_enable_device_mem(pdev)) {
		pr_info("0x%p failed to renable after slot reset\n", xpdev);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pci_set_master(pdev);
	pci_restore_state(pdev);
	pci_save_state(pdev);
	xdma_device_online(pdev, xpdev->xdev);

	return PCI_ERS_RESULT_RECOVERED;
}

static void xdma_error_resume(struct pci_dev *pdev)
{
	struct xdma_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p.\n", pdev, xpdev);
#if PCI_AER_NAMECHANGE
	pci_aer_clear_nonfatal_status(pdev);
#else
	pci_cleanup_aer_uncorrect_error_status(pdev);
#endif
}

#if KERNEL_VERSION(4, 13, 0) <= LINUX_VERSION_CODE
static void xdma_reset_prepare(struct pci_dev *pdev)
{
	struct xdma_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p.\n", pdev, xpdev);
	xdma_device_offline(pdev, xpdev->xdev);
}

static void xdma_reset_done(struct pci_dev *pdev)
{
	struct xdma_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p.\n", pdev, xpdev);
	xdma_device_online(pdev, xpdev->xdev);
}

#elif KERNEL_VERSION(3, 16, 0) <= LINUX_VERSION_CODE
static void xdma_reset_notify(struct pci_dev *pdev, bool prepare)
{
	struct xdma_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p, prepare %d.\n", pdev, xpdev, prepare);

	if (prepare)
		xdma_device_offline(pdev, xpdev->xdev);
	else
		xdma_device_online(pdev, xpdev->xdev);
}
#endif

static const struct pci_error_handlers xdma_err_handler = {
	.error_detected	= xdma_error_detected,
	.slot_reset	= xdma_slot_reset,
	.resume		= xdma_error_resume,
#if KERNEL_VERSION(4, 13, 0) <= LINUX_VERSION_CODE
	.reset_prepare	= xdma_reset_prepare,
	.reset_done	= xdma_reset_done,
#elif KERNEL_VERSION(3, 16, 0) <= LINUX_VERSION_CODE
	.reset_notify	= xdma_reset_notify,
#endif
};

static struct pci_driver pci_driver = {
	.name = DRV_MODULE_NAME,
	.id_table = pci_ids,
	.probe = probe_one,
	.remove = remove_one,
	.err_handler = &xdma_err_handler,
};

static int xdma_mod_init(void)
{
	int rv;
	pr_info("%s", version);

	if (desc_blen_max > XDMA_DESC_BLEN_MAX)
		desc_blen_max = XDMA_DESC_BLEN_MAX;
	pr_info("desc_blen_max: 0x%x/%u, timeout: h2c %u c2h %u sec.\n",
		desc_blen_max, desc_blen_max, h2c_timeout, c2h_timeout);

	rv = xdma_cdev_init();
	if (rv < 0)
		return rv;

	return pci_register_driver(&pci_driver);
}

static void xdma_mod_exit(void)
{
	/* unregister this driver from the PCI bus driver */
	dbg_init("pci_unregister_driver.\n");
	pci_unregister_driver(&pci_driver);
	xdma_cdev_cleanup();
}

module_init(xdma_mod_init);
module_exit(xdma_mod_exit);
