/**
 * Copyright (c) 2014 Himanshu Chauhan.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file ide_core.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief IDE core framework implementation.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_delay.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_modules.h>
#include <vmm_mutex.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <drv/ide/ide_core.h>

#define MODULE_DESC			"IDE Framework"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		IDE_CORE_IPRIORITY
#define	MODULE_INIT			ide_core_init
#define	MODULE_EXIT			ide_core_exit

/*
 * Protected list of ide hosts.
 */
static DEFINE_MUTEX(ide_drive_list_mutex);
static LIST_HEAD(ide_drive_list);
static u32 ide_drive_count;

static char *drive_names[MAX_IDE_CHANNELS][MAX_IDE_DRIVES_PER_CHAN] = {
	{
		"hda0",
		"hda1",
	},
	{
		"hda2",
		"hda3",
	}
};

static int ide_make_request(struct vmm_request_queue *rq, 
			    struct vmm_request *r);
static int ide_abort_request(struct vmm_request_queue *rq, 
			     struct vmm_request *r);

static int __init_ide_drive(struct ide_drive *drive)
{
	int rc = VMM_OK;
	struct vmm_blockdev *bdev;
	u8 chan, did;

	if (!drive) {
		return VMM_EFAIL;
	}

	if (drive->bdev)
		return VMM_OK;

	/* Allocate new block device instance */
	drive->bdev = vmm_blockdev_alloc();
	if (!drive->bdev) {
		rc = VMM_ENOMEM;
		goto detect_freecard_fail;
	}
	bdev = drive->bdev;

	chan = drive->channel->id;
	did = drive->drive;

	/* Setup block device instance */
	vmm_snprintf(bdev->name, sizeof(bdev->name),
		     "%s", drive_names[chan][did]);
	vmm_snprintf(bdev->desc, sizeof(bdev->desc),
		     "%s", drive->model);

	bdev->dev.parent = drive->dev;
	bdev->flags = VMM_BLOCKDEV_RW;
	bdev->start_lba = 0;
	bdev->block_size = drive->blk_size;
	bdev->num_blocks = drive->size;

	/* Setup request queue for block device instance */
	bdev->rq = vmm_zalloc(sizeof(struct vmm_request_queue));
	if (!bdev->rq) {
		goto detect_freebdev_fail;
	}
	INIT_REQUEST_QUEUE(bdev->rq);
	bdev->rq->make_request = ide_make_request;
	bdev->rq->abort_request = ide_abort_request;
	bdev->rq->priv = drive;

	rc = vmm_blockdev_register(drive->bdev);
	if (rc) {
		goto detect_freerq_fail;
	}

	rc = VMM_OK;
	goto detect_done;

detect_freerq_fail:
	vmm_free(drive->bdev->rq);
detect_freebdev_fail:
	vmm_blockdev_free(drive->bdev);
detect_freecard_fail:
detect_done:
	return rc;
}

static u32 __ide_bwrite(struct ide_drive *drive, u64 start,
			u32 blkcnt, const void *src)
{
	return drive->io_ops.block_write(drive, start, blkcnt, src);
}

static u32 __ide_bread(struct ide_drive *drive, u64 start,
		       u32 blkcnt, void *dst)
{
	return drive->io_ops.block_read(drive, start, blkcnt, dst);
}

static int __ide_blockdev_request(struct ide_drive *drive,
				  struct vmm_request_queue *rq,
				  struct vmm_request *r)
{
	int rc;
	u32 cnt;

	if (!r) {
		return VMM_EFAIL;
	}

	if (!drive || !rq) {
		vmm_blockdev_fail_request(r);
		return VMM_EFAIL;
	}

	switch (r->type) {
	case VMM_REQUEST_READ:
		cnt = __ide_bread(drive, r->lba, r->bcnt, r->data);
		if (cnt == r->bcnt) {
			vmm_blockdev_complete_request(r);
			rc = VMM_OK;
		} else {
			vmm_blockdev_fail_request(r);
			rc = VMM_EIO;
		}
		break;
	case VMM_REQUEST_WRITE:
		cnt = __ide_bwrite(drive, r->lba, r->bcnt, r->data);
		if (cnt == r->bcnt) {
			vmm_blockdev_complete_request(r);
			rc = VMM_OK;
		} else {
			vmm_blockdev_fail_request(r);
			rc = VMM_EIO;
		}
		break;
	default:
		vmm_blockdev_fail_request(r);
		rc = VMM_EFAIL;
		break;
	};

	return rc;
}

static int ide_io_thread(void *tdata)
{
	irq_flags_t f;
	struct dlist *l;
	struct ide_drive_io *io;
	struct ide_drive *drive = (struct ide_drive *)tdata;

	while (1) {
		if (vmm_completion_wait(&drive->io_avail) != VMM_OK) {
			vmm_printf("Failed to wait on completion.\n");
			return VMM_EFAIL;
		}

		vmm_spin_lock_irqsave(&drive->io_list_lock, f);
		if (list_empty(&drive->io_list)) {
			vmm_spin_unlock_irqrestore(&drive->io_list_lock, f);
			continue;
		}
		l = list_pop(&drive->io_list);
		vmm_spin_unlock_irqrestore(&drive->io_list_lock, f);

		io = list_entry(l, struct ide_drive_io, head);

		vmm_mutex_lock(&drive->lock);
		__ide_blockdev_request(drive, io->rq, io->r);
		vmm_mutex_unlock(&drive->lock);

		vmm_free(io);
	}

	return VMM_OK;
}

static int ide_make_request(struct vmm_request_queue *rq,
			    struct vmm_request *r)
{
	irq_flags_t f;
	struct ide_drive_io *io;
	struct ide_drive *drive;

	if (!r || !rq || !rq->priv) {
		return VMM_EFAIL;
	}

	drive = rq->priv;

	io = vmm_zalloc(sizeof(struct ide_drive_io));
	if (!io) {
		return VMM_ENOMEM;
	}

	INIT_LIST_HEAD(&io->head);
	io->rq = rq;
	io->r = r;

	vmm_spin_lock_irqsave(&drive->io_list_lock, f);
	list_add_tail(&io->head, &drive->io_list);
	vmm_spin_unlock_irqrestore(&drive->io_list_lock, f);

	vmm_completion_complete(&drive->io_avail);

	return VMM_OK;
}

static int ide_abort_request(struct vmm_request_queue *rq,
			     struct vmm_request *r)
{
	bool found;
	irq_flags_t f;
	struct dlist *l;
	struct ide_drive_io *io;
	struct ide_drive *drive;

	if (!r || !rq || !rq->priv) {
		return VMM_EFAIL;
	}

	drive = rq->priv;

	vmm_spin_lock_irqsave(&drive->io_list_lock, f);

	found = FALSE;
	list_for_each(l, &drive->io_list) {
		io = list_entry(l, struct ide_drive_io, head);
		if (io->r == r && io->rq == rq) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		list_del(&io->head);
		vmm_free(io);
	}

	vmm_spin_unlock_irqrestore(&drive->io_list_lock, f);
	
	return VMM_OK;
}

static vmm_irq_return_t handle_ata_interrupt(int irq_no, void *dev)
{
	struct ide_drive *drive = (struct ide_drive *)dev;

	vmm_completion_complete(&drive->dev_intr);

	return VMM_IRQ_HANDLED;
}

int ide_add_drive(struct ide_drive *drive)
{
	char name[32];

	if (!drive || drive->io_thread) {
		return VMM_EFAIL;
	}


	vmm_mutex_lock(&ide_drive_list_mutex);

	drive->io_thread = NULL;
	INIT_LIST_HEAD(&drive->link);
	INIT_LIST_HEAD(&drive->io_list);
	INIT_SPIN_LOCK(&drive->io_list_lock);
	INIT_COMPLETION(&drive->io_avail);
	INIT_COMPLETION(&drive->dev_intr);
	INIT_MUTEX(&drive->lock);

	if (__init_ide_drive(drive) != VMM_OK) {
		vmm_mutex_unlock(&ide_drive_list_mutex);
		vmm_printf("%s: IDE Block layer int failed\n", __func__);
		return VMM_EFAIL;
	}

	vmm_snprintf(name, 32, "%s",
		     drive_names[drive->channel->id][drive->drive]);
	drive->io_thread = vmm_threads_create(name, ide_io_thread, drive,
					      VMM_THREAD_DEF_PRIORITY,
					      VMM_THREAD_DEF_TIME_SLICE);
	if (!drive->io_thread) {
		vmm_mutex_unlock(&ide_drive_list_mutex);
		return VMM_EFAIL;
	}

	if (drive->channel->id)
		vmm_host_irq_register(SECONDARY_ATA_CHANNEL_IRQ,
				      "ATA-15", handle_ata_interrupt,
				      drive);
	else
		vmm_host_irq_register(PRIMARY_ATA_CHANNEL_IRQ,
				      "ATA-14", handle_ata_interrupt,
				      drive);

	ide_drive_count++;
	list_add_tail(&drive->link, &ide_drive_list);

	vmm_mutex_unlock(&ide_drive_list_mutex);

	vmm_threads_start(drive->io_thread);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(ide_add_drive);

static int __init ide_core_init(void)
{
	/* Nothing to be done until ATA core registers a drive */
	return VMM_OK;
}

static void __exit ide_core_exit(void)
{
	/* Nothing to be done. */
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
