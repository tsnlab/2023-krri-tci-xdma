/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 *
 * Copyright (c) 2017-present,  Xilinx, Inc.
 * All rights reserved.
 *
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

#define pr_fmt(fmt)    KBUILD_MODNAME ":%s: " fmt, __func__

#include "xdma_thread.h"

#include <asm/cacheflush.h>

#include <linux/kernel.h>
#include <linux/slab.h>


/* ********************* global variables *********************************** */
static struct xdma_kthread *cs_threads;
static unsigned int thread_cnt;


/* ********************* static function definitions ************************ */
static int xdma_thread_cmpl_status_pend(struct list_head *work_item)
{
    struct xdma_engine *engine = list_entry(work_item, struct xdma_engine,
                        cmplthp_list);
    int pend = 0;
    unsigned long flags;

    spin_lock_irqsave(&engine->lock, flags);
    pend = !list_empty(&engine->transfer_list);
    spin_unlock_irqrestore(&engine->lock, flags);

    return pend;
}

static int xdma_thread_cmpl_status_proc(struct list_head *work_item)
{
    struct xdma_engine *engine;
    struct xdma_transfer * transfer;

    engine = list_entry(work_item, struct xdma_engine, cmplthp_list);
    transfer = list_entry(engine->transfer_list.next, struct xdma_transfer,
            entry);
    if (transfer)
        engine_service_poll(engine, transfer->desc_cmpl_th);
    return 0;
}

static inline int xthread_work_pending(struct xdma_kthread *thp)
{
    struct list_head *work_item, *next;

    /* any work items assigned to this thread? */
    if (list_empty(&thp->work_list))
        return 0;

    /* any work item has pending work to do? */
    list_for_each_safe(work_item, next, &thp->work_list) {
        if (thp->fpending && thp->fpending(work_item))
            return 1;

    }
    return 0;
}

static inline void xthread_reschedule(struct xdma_kthread *thp)
{
    if (thp->timeout) {
        pr_debug_thread("%s rescheduling for %u seconds",
                thp->name, thp->timeout);
        wait_event_interruptible_timeout(thp->waitq, thp->schedule,
                          msecs_to_jiffies(thp->timeout));
    } else {
        pr_debug_thread("%s rescheduling", thp->name);
        wait_event_interruptible(thp->waitq, thp->schedule);
    }
}

static int xthread_main(void *data)
{
    struct xdma_kthread *thp = (struct xdma_kthread *)data;

    pr_debug_thread("%s UP.\n", thp->name);

    disallow_signal(SIGPIPE);

    if (thp->finit)
        thp->finit(thp);

    while (!kthread_should_stop()) {

        struct list_head *work_item, *next;

        pr_debug_thread("%s interruptible\n", thp->name);

        /* any work to do? */
        lock_thread(thp);
        if (!xthread_work_pending(thp)) {
            unlock_thread(thp);
            xthread_reschedule(thp);
            lock_thread(thp);
        }
        thp->schedule = 0;

        if (thp->work_cnt) {
            pr_debug_thread("%s processing %u work items\n",
                    thp->name, thp->work_cnt);
            /* do work */
            list_for_each_safe(work_item, next, &thp->work_list) {
                thp->fproc(work_item);
            }
        }
        unlock_thread(thp);
        schedule();
    }

    pr_debug_thread("%s, work done.\n", thp->name);

    if (thp->fdone)
        thp->fdone(thp);

    pr_debug_thread("%s, exit.\n", thp->name);
    return 0;
}


int xdma_kthread_start(struct xdma_kthread *thp, char *name, int id)
{
    int len;
    int node;

    if (thp->task) {
        pr_warn("kthread %s task already running?\n", thp->name);
        return -EINVAL;
    }

    len = snprintf(thp->name, sizeof(thp->name), "%s%d", name, id);
    if (len < 0) {
        pr_err("thread %d, error in snprintf name %s.\n", id, name);
        return -EINVAL;
    }

    thp->id = id;

    spin_lock_init(&thp->lock);
    INIT_LIST_HEAD(&thp->work_list);
    init_waitqueue_head(&thp->waitq);

    node = cpu_to_node(thp->cpu);
    pr_debug("node : %d\n", node);

    thp->task = kthread_create_on_node(xthread_main, (void *)thp,
                    node, "%s", thp->name);
    if (IS_ERR(thp->task)) {
        pr_err("kthread %s, create task failed: 0x%lx\n",
            thp->name, (unsigned long)IS_ERR(thp->task));
        thp->task = NULL;
        return -EFAULT;
    }

    kthread_bind(thp->task, thp->cpu);

    pr_debug_thread("kthread 0x%p, %s, cpu %u, task 0x%p.\n",
        thp, thp->name, thp->cpu, thp->task);

    wake_up_process(thp->task);
    return 0;
}

int xdma_kthread_stop(struct xdma_kthread *thp)
{
    int rv;

    if (!thp->task) {
        pr_debug_thread("kthread %s, already stopped.\n", thp->name);
        return 0;
    }

    thp->schedule = 1;
    rv = kthread_stop(thp->task);
    if (rv < 0) {
        pr_warn("kthread %s, stop err %d.\n", thp->name, rv);
        return rv;
    }

    pr_debug_thread("kthread %s, 0x%p, stopped.\n", thp->name, thp->task);
    thp->task = NULL;

    return 0;
}



void xdma_thread_remove_work(struct xdma_engine *engine)
{
    struct xdma_kthread *cmpl_thread;
    unsigned long flags;

    spin_lock_irqsave(&engine->lock, flags);
    cmpl_thread = engine->cmplthp;
    engine->cmplthp = NULL;

//    pr_debug("%s removing from thread %s, %u.\n",
//        descq->conf.name, cmpl_thread ? cmpl_thread->name : "?",
//        cpu_idx);

    spin_unlock_irqrestore(&engine->lock, flags);

#if 0
    if (cpu_idx < cpu_count) {
        spin_lock(&qcnt_lock);
        per_cpu_qcnt[cpu_idx]--;
        spin_unlock(&qcnt_lock);
    }
#endif

    if (cmpl_thread) {
        lock_thread(cmpl_thread);
        list_del(&engine->cmplthp_list);
        cmpl_thread->work_cnt--;
        unlock_thread(cmpl_thread);
    }
}

void xdma_thread_add_work(struct xdma_engine *engine)
{
    struct xdma_kthread *thp = cs_threads;
    unsigned int v = 0;
    int i, idx = thread_cnt;
    unsigned long flags;


    /* Polled mode only */
    for (i = 0; i < thread_cnt; i++, thp++) {
        lock_thread(thp);
        if (idx == thread_cnt) {
            v = thp->work_cnt;
            idx = i;
        } else if (!thp->work_cnt) {
            idx = i;
            unlock_thread(thp);
            break;
        } else if (thp->work_cnt < v)
            idx = i;
        unlock_thread(thp);
    }

    thp = cs_threads + idx;
    lock_thread(thp);
    list_add_tail(&engine->cmplthp_list, &thp->work_list);
    engine->intr_work_cpu = idx;
    thp->work_cnt++;
    unlock_thread(thp);

    pr_info("%s 0x%p assigned to cmpl status thread %s,%u.\n",
        engine->name, engine, thp->name, thp->work_cnt);


    spin_lock_irqsave(&engine->lock, flags);
    engine->cmplthp = thp;
    spin_unlock_irqrestore(&engine->lock, flags);
}

int xdma_threads_create(unsigned int num_threads)
{
    struct xdma_kthread *thp;
    int rv;
    int cpu;

    if (thread_cnt) {
        pr_warn("threads already created!");
        return 0;
    }

    cs_threads = kzalloc(num_threads * sizeof(struct xdma_kthread),
                    GFP_KERNEL);
    if (!cs_threads) {
        pr_err("OOM, # threads %u.\n", num_threads);
        return -ENOMEM;
    }

    /* N dma writeback monitoring threads */
    thp = cs_threads;
    for_each_online_cpu(cpu) {
        pr_debug("index %d cpu %d online\n", thread_cnt, cpu);
        thp->cpu = cpu;
        thp->timeout = 0;
        thp->fproc = xdma_thread_cmpl_status_proc;
        thp->fpending = xdma_thread_cmpl_status_pend;
        rv = xdma_kthread_start(thp, "cmpl_status_th", thread_cnt);
        if (rv < 0)
            goto cleanup_threads;

        thread_cnt++;
        if (thread_cnt == num_threads)
            break;
        thp++;
    }

    return 0;

cleanup_threads:
    kfree(cs_threads);
    cs_threads = NULL;
    thread_cnt = 0;

    return rv;
}

void xdma_threads_destroy(void)
{
    int i;
    struct xdma_kthread *thp;

    if (!thread_cnt)
        return;

    /* N dma writeback monitoring threads */
    thp = cs_threads;
    for (i = 0; i < thread_cnt; i++, thp++)
        if (thp->fproc)
            xdma_kthread_stop(thp);

    kfree(cs_threads);
    cs_threads = NULL;
    thread_cnt = 0;
}

#include <linux/delay.h>

static void tsn_char_sgdma_unmap_user_buf(struct xdma_io_cb *cb, bool write)
{
    int i;

    sg_free_table(&cb->sgt);

    if (!cb->pages || !cb->pages_nr)
        return;

    for (i = 0; i < cb->pages_nr; i++) {
        if (cb->pages[i]) {
            if (!write)
                set_page_dirty_lock(cb->pages[i]);
            put_page(cb->pages[i]);
        } else
            break;
    }

    if (i != cb->pages_nr)
        pr_info("sgl pages %d/%u.\n", i, cb->pages_nr);

    kfree(cb->pages);
    cb->pages = NULL;
}

static int tsn_char_sgdma_map_user_buf_to_sgl(struct xdma_io_cb *cb, bool write)
{
    struct sg_table *sgt = &cb->sgt;
    unsigned long len = cb->len;
    void __user *buf = cb->buf;
    struct scatterlist *sg;
    unsigned int pages_nr = (((unsigned long)buf + len + PAGE_SIZE - 1) -
                 ((unsigned long)buf & PAGE_MASK))
                >> PAGE_SHIFT;
    int i;
    int rv;

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

    rv = get_user_pages_fast((unsigned long)buf, pages_nr, 1/* write */,
                cb->pages);
    /* No pages were pinned */
    if (rv < 0) {
        pr_err("unable to pin down %u user pages, %d.\n",
            pages_nr, rv);
        goto err_out;
    }
    /* Less pages pinned than wanted */
    if (rv != pages_nr) {
        pr_err("unable to pin down all %u user pages, %d.\n",
            pages_nr, rv);
        cb->pages_nr = rv;
        rv = -EFAULT;
        goto err_out;
    }

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
        unsigned int offset = offset_in_page(buf);
        unsigned int nbytes =
            min_t(unsigned int, PAGE_SIZE - offset, len);

        flush_dcache_page(cb->pages[i]);
        sg_set_page(sg, cb->pages[i], nbytes, offset);

        buf += nbytes;
        len -= nbytes;
    }

    if (len) {
        pr_err("Invalid user buffer length. Cannot map to sgl\n");
        return -EINVAL;
    }
    cb->pages_nr = pages_nr;
    printk(KERN_INFO ">>> %s - %d\n", __func__, __LINE__);
    return 0;

err_out:
    tsn_char_sgdma_unmap_user_buf(cb, write);

    return rv;
}

extern unsigned int h2c_timeout;
extern unsigned int c2h_timeout;

ssize_t xdma_xfer_submit(void *dev_hndl, int channel, bool write, u64 ep_addr,
             struct sg_table *sgt, bool dma_mapped, int timeout_ms);
static ssize_t tsn_char_sgdma_read_write(struct tsn_kthread *tsnthp, const char __user *buf,
        size_t count, loff_t *pos, bool write)
{
    int rv;
    ssize_t res = 0;
    struct xdma_dev *xdev;
    struct xdma_io_cb cb;
    struct xdma_engine *engine;

    engine = tsnthp->engine;
    xdev = tsnthp->xdev;
    memset(&cb, 0, sizeof(struct xdma_io_cb));
    cb.buf = (char __user *)buf;
    cb.len = count;
    cb.ep_addr = (u64)*pos;
    cb.write = write;
    rv = tsn_char_sgdma_map_user_buf_to_sgl(&cb, write);
    if (rv < 0)
        return rv;

    res = xdma_xfer_submit(xdev, engine->channel, write, *pos, &cb.sgt,
                0, c2h_timeout * 1000);

    tsn_char_sgdma_unmap_user_buf(&cb, write);

    return res;
}

static int tsn_dma_to_device_thread_func(void *data)
{
    struct tsn_kthread *tsnthp;

    tsnthp = (struct tsn_kthread *)data;

    while (!kthread_should_stop()) {
        printk(KERN_INFO "%s %s is running\n", tsnthp->name, __func__);
        msleep(1000); 
    }
    return 0;
}

static int tsn_dma_from_device_thread_func(void *data)
{
    struct tsn_kthread *tsnthp = NULL;
    int id;
    int run = 1;
    ssize_t res;
    struct XDma_Bd *bdp;

    tsnthp = (struct tsn_kthread *)data;;

    bdp = tsnthp->BdRing.FreeHead;

    while (!kthread_should_stop()) {

        if(run) {
            for(id=0; id<tsnthp->BdRing.FreeCnt; id++) {
                res = tsn_char_sgdma_read_write(tsnthp, bdp[id].s_Bd.buffer, 1, 0, 0);
                printk(KERN_INFO "bdp[%4d]: %5d bytes were received\n", id, (int)res);
            }
            run = 0;
        }
        printk(KERN_INFO "%s %s is running\n", tsnthp->name, __func__);
        msleep(1000); 
    }
    return 0;
}

int tsn_thread_start(struct file *file, struct xdma_engine *engine)
{
    struct xdma_dev *xdev;
    struct pci_dev *pdev;    /* pci device struct from probe() */
    struct tsn_kthread *tsnthp = engine->tsnthp;

    if (tsnthp == NULL) {
        pr_err("%s thread is not initialized!\n", engine->name);
        return -EINVAL;
    }

    if (tsnthp->running) {
        pr_err("%s thread is already running!\n", engine->name);
        return 0;
    }

    xdev = kzalloc(sizeof(struct xdma_dev), GFP_KERNEL);
    if (!xdev) {
        pr_err("OOM, xdev %u.\n", (unsigned int)sizeof(struct xdma_dev));
        return -ENOMEM;
    }
    memcpy(xdev, engine->xdev, sizeof(struct xdma_dev));

    pdev = kzalloc(sizeof(struct pci_dev), GFP_KERNEL);
    if (!pdev) {
        kfree(xdev);
        pr_err("OOM, pdev %u.\n", (unsigned int)sizeof(struct pci_dev));
        return -ENOMEM;
    }

    if(engine->dir == DMA_TO_DEVICE) {
        tsnthp->my_thread = kthread_run(tsn_dma_to_device_thread_func, tsnthp, engine->name);
    } else {
        tsnthp->my_thread = kthread_run(tsn_dma_from_device_thread_func, tsnthp, engine->name);
    }

    if (IS_ERR(tsnthp->my_thread)) {
        tsnthp->running = 0;
        printk(KERN_ERR "Failed to start kernel thread\n");
        kfree(pdev);
        kfree(xdev);
        return PTR_ERR(tsnthp->my_thread);
    }
    tsnthp->xdev = xdev;
    tsnthp->xdev->pdev = pdev;
    tsnthp->running = 1;
    printk(KERN_INFO "%s thread started\n", tsnthp->name);
    return 0;
}

void tsn_thread_stop(struct xdma_engine *engine)
{
    struct tsn_kthread *tsnthp = engine->tsnthp;

    if (tsnthp == NULL) {
        pr_err("%s thread is not initialized!\n", engine->name);
        return;
    }

    if (tsnthp->xdev != NULL) {
        kfree(tsnthp->xdev);
    }

    if (tsnthp->running == 0) {
        pr_err("%s thread is already stopped!\n", tsnthp->name);
        return;
    }

    kthread_stop(tsnthp->my_thread);
    tsnthp->running = 0;
    tsnthp->xdev = NULL;
    printk(KERN_INFO "%s thread stopped\n", tsnthp->name);
}

int tsn_thread_init(struct xdma_engine *engine, int BufferCount)
{
    struct tsn_kthread *tsnthp;
    struct XDma_Bd *bdp;
    int idx;

    if (engine->tsnthp) {
        pr_err("%s thread is already created!\n", engine->name);
        return -EINVAL;
    }

    bdp = kzalloc(BufferCount * sizeof(struct XDma_Bd), GFP_KERNEL);
    if (!bdp) {
        pr_err("OOM, # buffer descriptor %u.\n", BufferCount);
        return -ENOMEM;
    }
    for(idx=0; idx < (BufferCount - 1); idx++) {
        bdp[idx].s_Bd.nextBd = (struct XDma_Bd *)&bdp[idx+1];
        bdp[idx].s_Bd.status = 0;
        bdp[idx].s_Bd.id = idx;
    }
    bdp[idx].s_Bd.nextBd = (struct XDma_Bd *)&bdp[0];
    bdp[idx].s_Bd.status = 0;
    bdp[idx].s_Bd.id = idx;
    
    tsnthp = kzalloc(1 * sizeof(struct tsn_kthread), GFP_KERNEL);
    if (!tsnthp) {
        pr_err("OOM, # tsn threads %u.\n", 1);
        kfree(bdp);
        return -ENOMEM;
    }

    tsnthp->engine = engine;
    tsnthp->BdRing.FreeHead = bdp;
    tsnthp->running = 0;
    tsnthp->BufferCount = BufferCount;
    tsnthp->BdRing.AllCnt = BufferCount;
    tsnthp->BdRing.FreeCnt = 0;

    sprintf(tsnthp->name, "TSN-%s",engine->name);
    engine->tsnthp = tsnthp;
    printk(KERN_INFO "%s thread is initialized, BufferCount: %d\n", 
                      tsnthp->name, tsnthp->BufferCount);
    return 0;
}

void tsn_thread_exit(struct xdma_engine *engine)
{
    struct tsn_kthread *tsnthp = engine->tsnthp;

    if (tsnthp == NULL) {
        pr_err("%s thread is already exited!\n", engine->name);
        return;
    }

    /* Do somethong */

    if(tsnthp->running == 1)
    {
        printk(KERN_INFO "%s thread stopped\n", tsnthp->name);
        kthread_stop(tsnthp->my_thread);
        tsnthp->running = 0;
    }

    if (tsnthp->xdev != NULL) {
        kfree(tsnthp->xdev);
    }
    kfree(tsnthp->BdRing.FreeHead);
    kfree(engine->tsnthp);
    engine->tsnthp = NULL;
    printk(KERN_INFO "%s thread is exited\n", engine->name);
}

int tsn_bd_set_buffer_address(struct xdma_engine *engine, unsigned long arg)
{
    struct tsn_kthread *tsnthp = engine->tsnthp;
    int rv;
    struct xdma_bd_set_bufAddr_ioctl bdSet;
    struct XDma_Bd *bdp;

    if(tsnthp->BdRing.FreeCnt >= tsnthp->BdRing.AllCnt) {
        pr_err("Failed to copy from user space 0x%lx\n", arg);
        return -EINVAL;
    }
    rv = copy_from_user(&bdSet,
        (struct xdma_bd_set_bufAddr_ioctl __user *)arg,
        sizeof(struct xdma_bd_set_bufAddr_ioctl));

    if (rv < 0) {
        pr_err("Failed to copy from user space 0x%lx\n", arg);
        return -EINVAL;
    }

    bdp = tsnthp->BdRing.FreeHead;
    bdp[bdSet.id].s_Bd.buffer = bdSet.buffer;
    bdp[bdSet.id].s_Bd.user_address = bdSet.user_address;
    tsnthp->BdRing.FreeCnt += 1;
    return 0;
}

int tsn_bd_get_buffer_address(struct xdma_engine *engine, unsigned long arg)
{
    struct tsn_kthread *tsnthp = engine->tsnthp;
    int rv;
    struct xdma_bd_get_bufAddr_ioctl bdGet1, bdGet2;
    struct XDma_Bd *bdp;

    rv = copy_from_user(&bdGet1,
        (struct xdma_bd_get_bufAddr_ioctl __user *)arg,
        sizeof(struct xdma_bd_get_bufAddr_ioctl));

    if (rv < 0) {
        pr_err("Failed to copy from user space 0x%lx\n", arg);
        return -EINVAL;
    }
    
    bdp = tsnthp->BdRing.FreeHead;
    bdGet2.id = bdGet1.id;
    bdGet2.buffer = bdp[bdGet1.id].s_Bd.buffer;
    bdGet2.user_address = bdp[bdGet1.id].s_Bd.user_address;

    rv = copy_to_user((void __user *)arg, &bdGet2,
            sizeof(struct xdma_bd_get_bufAddr_ioctl));
    if (rv) {
        dbg_perf("Error copying result to user\n");
        return rv;
    }

    return 0;
}

int tsn_bd_insert(struct xdma_engine *engine, unsigned long arg)
{
    unsigned long dst;
    int rv;

    rv = get_user(dst, (int __user *)arg);

    if (rv == 0) {
        printk(KERN_INFO "%s arg: %p, dst: %lx\n", engine->name, (void *)arg, dst);
    }

    printk(KERN_INFO "%s BD inserted\n", engine->name);
    return 0;
}
