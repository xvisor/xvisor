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
 * @file ata_core.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief IDE ATA framework
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_delay.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <asm/io.h>
#include <drv/ide/ide.h>
#include <drv/ide/ata.h>

static void ide_write(struct ide_channel *channel, u8 reg, u8 data)
{
	int port;

	if (reg > 0x07 && reg < 0x0C) {
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channel->int_en);
	}

	if (reg < 0x08) port = channel->base + reg - 0x00;
	else if (reg < 0x0c) port = channel->base + reg - 0x06;
	else if (reg < 0x0e) port = channel->ctrl + reg - 0x0a;
	else if (reg < 0x16) port = channel->bmide + reg - 0x0e;

	outb(data, port);

	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channel->int_en);
}

static u8 ide_read(struct ide_channel *channel, u8 reg)
{
	u8 result;

	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channel->int_en);
	if (reg < 0x08)
		result = inb(channel->base + reg - 0x00);
	else if (reg < 0x0C)
		result = inb(channel->base  + reg - 0x06);
	else if (reg < 0x0E)
		result = inb(channel->ctrl  + reg - 0x0A);
	else if (reg < 0x16)
		result = inb(channel->bmide + reg - 0x0E);
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channel->int_en);

	return result;
}

unsigned char ide_print_error(struct ide_drive *drive, unsigned char err)
{
	if (err == 0)
		return err;
 
	vmm_printf("IDE:");
	if (err == 1) { vmm_printf("- Device Fault\n     "); err = 19;}
	else if (err == 2) {
		unsigned char st = ide_read(drive->channel, ATA_REG_ERROR);
		if (st & ATA_ER_AMNF)   { vmm_printf("- No Address Mark Found\n     ");   err = 7;}
		if (st & ATA_ER_TK0NF)   { vmm_printf("- No Media or Media Error\n     ");   err = 3;}
		if (st & ATA_ER_ABRT)   { vmm_printf("- Command Aborted\n     ");      err = 20;}
		if (st & ATA_ER_MCR)   { vmm_printf("- No Media or Media Error\n     ");   err = 3;}
		if (st & ATA_ER_IDNF)   { vmm_printf("- ID mark not Found\n     ");      err = 21;}
		if (st & ATA_ER_MC)   { vmm_printf("- No Media or Media Error\n     ");   err = 3;}
		if (st & ATA_ER_UNC)   { vmm_printf("- Uncorrectable Data Error\n     ");   err = 22;}
		if (st & ATA_ER_BBK)   { vmm_printf("- Bad Sectors\n     ");       err = 13;}
	} else  if (err == 3)           {vmm_printf("- Reads Nothing\n     "); err = 23;}
	else  if (err == 4)  { vmm_printf("- Write Protected\n     "); err = 8;}

	vmm_printf("- [%s %s] %s\n",
	       (const char *[]){"Primary", "Secondary"}[drive->channel->id], // Use the channel as an index into the array
	       (const char *[]){"Master", "Slave"}[drive->drive], // Same as above, using the drive
	       drive->model);

	return err;
}

static void ide_read_buffer(struct ide_channel *channel, u8 reg, void *buffer, u32 quads)
{
	/* WARNING: This code contains a serious bug. The inline assembly trashes ES and
	 *           ESP for all of the code the compiler generates between the inline
	 *           assembly blocks.
	 */
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channel->int_en);
	if (reg < 0x08)
		insl(channel->base  + reg - 0x00, buffer, quads);
	else if (reg < 0x0C)
		insl(channel->base  + reg - 0x06, buffer, quads);
	else if (reg < 0x0E)
		insl(channel->ctrl  + reg - 0x0A, buffer, quads);
	else if (reg < 0x16)
		insl(channel->bmide + reg - 0x0E, buffer, quads);
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channel->int_en);
}

static u8 ide_polling(struct ide_channel *channel, u32 advanced_check)
{
	int i;

	/* Delay 400 nanosecond for BSY to be set: */
	/* Reading the Alternate Status port wastes 100ns; loop four times. */
	for(i = 0; i < 4; i++)
		ide_read(channel, ATA_REG_ALTSTATUS);

	/* Wait for BSY to be cleared: */
	while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY)
		;

	if (advanced_check) {
		unsigned char state = ide_read(channel, ATA_REG_STATUS);

		/* Check For Errors: */
		if (state & ATA_SR_ERR)
			return 2;

		/* Check If Device fault: */
		if (state & ATA_SR_DF)
			return 1;

		/* Check DRQ:
		 * BSY = 0; DF = 0; ERR = 0 so we should check for DRQ now. */
		if ((state & ATA_SR_DRQ) == 0)
			return 3;

	}

	return 0;
}

static u8 ide_ata_access(struct ide_drive *drive, u8 direction, u64 lba,
			 u32 numsects, void *buffer)
{
	u8 lba_mode; /* 0: CHS, 1:LBA28, 2: LBA48 */
	u8 dma = 0; /* 0: No DMA, 1: DMA */
	u8 cmd;
	u8 lba_io[6];
	struct ide_channel  *channel = drive->channel;
	u32  slavebit = drive->drive;
	u32  bus = channel->base;
	/* Almost every ATA drive has a sector-size of 512-byte */
	unsigned int  words = 256;
	unsigned short cyl, i;
	unsigned char head, sect, err;

	/* Select one from LBA28, LBA48 or CHS */
	if (lba >= 0x10000000) {
		/* giving a wrong LBA. LBA48: */
		lba_mode  = 2;
		lba_io[0] = (lba & 0x000000FF) >> 0;
		lba_io[1] = (lba & 0x0000FF00) >> 8;
		lba_io[2] = (lba & 0x00FF0000) >> 16;
		lba_io[3] = (lba & 0xFF000000) >> 24;
		lba_io[4] = 0; /* LBA28 is integer, so 32-bits are enough to access 2TB. */
		lba_io[5] = 0; /* LBA28 is integer, so 32-bits are enough to access 2TB. */
		head      = 0; /* Lower 4-bits of HDDEVSEL are not used here. */
	} else if (drive->capabilities & 0x200)  { /* Drive supports LBA? */
		/* LBA28: */
		lba_mode  = 1;
		lba_io[0] = (lba & 0x00000FF) >> 0;
		lba_io[1] = (lba & 0x000FF00) >> 8;
		lba_io[2] = (lba & 0x0FF0000) >> 16;
		lba_io[3] = 0;
		lba_io[4] = 0;
		lba_io[5] = 0;
		head      = (lba & 0xF000000) >> 24;
	} else {
		/* CHS */
		lba_mode  = 0;
		sect      = (lba % 63) + 1;
		cyl       = (lba + 1  - sect) / (16 * 63);
		lba_io[0] = sect;
		lba_io[1] = (cyl >> 0) & 0xFF;
		lba_io[2] = (cyl >> 8) & 0xFF;
		lba_io[3] = 0;
		lba_io[4] = 0;
		lba_io[5] = 0;
		head      = (lba + 1  - sect) % (16 * 63) / (63); /* Head number is written to HDDEVSEL lower 4-bits. */
	}

	/* Wait if the drive is busy; */
	while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY)
		; /* Wait if busy. */

	/* Select Drive from the controller; */
	if (lba_mode == 0)
		ide_write(channel, ATA_REG_HDDEVSEL, 0xA0 | (slavebit << 4) | head);
	else
		ide_write(channel, ATA_REG_HDDEVSEL, 0xE0 | (slavebit << 4) | head);

	/* Write Parameters; */
	if (lba_mode == 2) {
		ide_write(channel, ATA_REG_SECCOUNT1,   0);
		ide_write(channel, ATA_REG_LBA3,   lba_io[3]);
		ide_write(channel, ATA_REG_LBA4,   lba_io[4]);
		ide_write(channel, ATA_REG_LBA5,   lba_io[5]);
	}

	ide_write(channel, ATA_REG_SECCOUNT0,   numsects);
	ide_write(channel, ATA_REG_LBA0,   lba_io[0]);
	ide_write(channel, ATA_REG_LBA1,   lba_io[1]);
	ide_write(channel, ATA_REG_LBA2,   lba_io[2]);

	if (lba_mode == 0 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
	if (lba_mode == 1 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
	if (lba_mode == 2 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO_EXT;
	if (lba_mode == 0 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
	if (lba_mode == 1 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
	if (lba_mode == 2 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA_EXT;
	if (lba_mode == 0 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
	if (lba_mode == 1 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
	if (lba_mode == 2 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
	if (lba_mode == 0 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
	if (lba_mode == 1 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
	if (lba_mode == 2 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA_EXT;
	ide_write(channel, ATA_REG_COMMAND, cmd);

	if (direction == 0)
		/* PIO Read. */
		for (i = 0; i < numsects; i++) {
			if ((err = ide_polling(channel, 1)))
				return err; /* Polling, set error and exit if there is. */

			insw(bus, buffer, words);
			buffer += words * sizeof(u16);
		} else {
		/* PIO Write. */
		for (i = 0; i < numsects; i++) {
			ide_polling(channel, 0);
			outsw(bus, buffer, words);
			buffer += words * sizeof(u16);
		}
		ide_write(channel, ATA_REG_COMMAND, (char []) {   ATA_CMD_CACHE_FLUSH,
					ATA_CMD_CACHE_FLUSH,
					ATA_CMD_CACHE_FLUSH_EXT}[lba_mode]);
		ide_polling(channel, 0);
	}

	return 0;
}

u32 ide_write_sectors(struct ide_drive *drive, u64 lba, u32 numsects, const void *buffer)
{
	/* Check if the drive presents: */
	if (drive->drive > 3 || drive->present == 0)
		return 0;

	/* Check if inputs are valid: */
	else if (((lba + numsects) > drive->size) && (drive->type == IDE_ATA))
		return 0;

	/* Read in PIO Mode through Polling & IRQs: */
	else {
		u8 err;
		if (drive->type == IDE_ATA)
			err = ide_ata_access(drive, ATA_WRITE, lba, numsects, (void *)buffer);

		if (err) {
			ide_print_error(drive, err);
			return 0;
		}
		return numsects;
	}

	return numsects;
}

u32 ide_read_sectors(struct ide_drive *drive, u64 lba, u32 numsects, void *buffer)
{
	/* Check if the drive presents: */
	if (drive->drive > 3 || drive->present == 0) return 0;

	/* Check if inputs are valid: */
	else if (((lba + numsects) > drive->size) && (drive->type == IDE_ATA))
		return 0;

	/* Read in PIO Mode through Polling & IRQs: */
	else {
		u8 err;
		if (drive->type == IDE_ATA)
			err = ide_ata_access(drive, ATA_READ, lba, numsects, buffer);

		ide_print_error(drive, err);
	}

	return numsects;
}

int ide_initialize(struct ide_host_controller *controller)
{
	int i, j, k, count = 0;
	u8 err, type = IDE_ATA;
	volatile u8 status;
	u8 ide_buf[512];

	controller->ide_channels[ATA_PRIMARY].id = ATA_PRIMARY;
	controller->ide_channels[ATA_PRIMARY].int_en = 1;
	controller->ide_channels[ATA_SECONDARY].id = ATA_SECONDARY;
	controller->ide_channels[ATA_SECONDARY].int_en = 1;

	/* Detect I/O Ports which interface IDE Controller: */
	controller->ide_channels[ATA_PRIMARY  ].base  = (controller->bar0 & 0xFFFFFFFC) + 0x1F0 * (!controller->bar0);
	controller->ide_channels[ATA_PRIMARY  ].ctrl  = (controller->bar1 & 0xFFFFFFFC) + 0x3F6 * (!controller->bar1);
	controller->ide_channels[ATA_SECONDARY].base  = (controller->bar2 & 0xFFFFFFFC) + 0x170 * (!controller->bar2);
	controller->ide_channels[ATA_SECONDARY].ctrl  = (controller->bar3 & 0xFFFFFFFC) + 0x376 * (!controller->bar3);
	controller->ide_channels[ATA_PRIMARY  ].bmide = (controller->bar4 & 0xFFFFFFFC) + 0; // Bus Master IDE
	controller->ide_channels[ATA_SECONDARY].bmide = (controller->bar4 & 0xFFFFFFFC) + 8; // Bus Master IDE

	/* Disable IRQs: */
	ide_write(&controller->ide_channels[ATA_PRIMARY], ATA_REG_CONTROL, 2);
	ide_write(&controller->ide_channels[ATA_SECONDARY], ATA_REG_CONTROL, 2);

	/* Detect ATA-ATAPI Devices: */
	for (i = 0; i < MAX_IDE_CHANNELS; i++) {
		for (j = 0; j < MAX_IDE_DRIVES_PER_CHAN; j++) {
			controller->ide_drives[count].present = 0; /* Assuming that no drive here. */

			/* Select Drive: */
			ide_write(&controller->ide_channels[i], ATA_REG_HDDEVSEL, 0xA0 | (j << 4)); /* Select Drive. */
			vmm_mdelay(1); /* Wait 1ms for drive select to work. */

			/* Send ATA Identify Command: */
			ide_write(&controller->ide_channels[i], ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
			vmm_mdelay(1); /* This function should be implemented in your OS. which waits for 1 ms. */

			/* Polling: */
			if (ide_read(&controller->ide_channels[i], ATA_REG_STATUS) == 0) {
				continue; /* If Status = 0, No Device. */
			}

			while(1) {
				status = ide_read(&controller->ide_channels[i], ATA_REG_STATUS);

				if ((status & ATA_SR_ERR)) {
					err = 1;
					break;
				} /* If Err, Device is not ATA. */

				if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;
			}

			if (err) {
				continue;
			}

			/* Read Identification Space of the Device: */
			ide_read_buffer(&controller->ide_channels[i], ATA_REG_DATA, (void *)ide_buf, 128);

			/* Read Device Parameters: */
			controller->ide_drives[count].present      = 1;
			controller->ide_drives[count].type         = type;
			controller->ide_drives[count].channel      = &controller->ide_channels[i];
			controller->ide_drives[count].drive        = j;
			controller->ide_drives[count].signature    = *((unsigned short *)(ide_buf + ATA_IDENT_DEVICETYPE));
			controller->ide_drives[count].capabilities = *((unsigned short *)(ide_buf + ATA_IDENT_CAPABILITIES));
			controller->ide_drives[count].cmd_set      = *((unsigned int *)(ide_buf + ATA_IDENT_COMMANDSETS));
			controller->ide_drives[count].io_ops.block_read = ide_read_sectors;
			controller->ide_drives[count].io_ops.block_write = ide_write_sectors;

			/* Get Size: */
			if (controller->ide_drives[count].cmd_set & (1 << 26)) {
				/* Device uses 48-Bit Addressing: */
				controller->ide_drives[count].size   = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA_EXT));
				controller->ide_drives[count].lba48_enabled = 1;
			} else {
				/* Device uses CHS or 28-bit Addressing: */
				controller->ide_drives[count].size   = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA));
				controller->ide_drives[count].lba48_enabled = 0;
			}

			/* String indicates model of device (like Western Digital HDD and SONY DVD-RW...): */
			for(k = 0; k < 40; k += 2) {
				controller->ide_drives[count].model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
				controller->ide_drives[count].model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
			}
			controller->ide_drives[count].model[40] = 0; // Terminate String.

			count++;
		}
	}

	return VMM_OK;
}

VMM_EXPORT_SYMBOL_GPL(ide_initialize);
VMM_EXPORT_SYMBOL_GPL(ide_read_sectors);
VMM_EXPORT_SYMBOL_GPL(ide_write_sectors);
