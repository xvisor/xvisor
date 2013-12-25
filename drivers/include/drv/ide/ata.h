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
 * @file ata.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief IDE ATA framework header file.
 */

#ifndef	_ATA_H
#define _ATA_H

/* Channels */
#define ATA_PRIMARY	0x00
#define ATA_SECONDARY	0x01

/* Direction */
#define ATA_READ	0x00
#define ATA_WRITE	0x01

#define ATA_MASTER	0x00
#define ATA_SLAVE	0x01

/*
 * I/O Register Descriptions
 */
#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

/*
 * Status register bits
 */
#define ATA_STAT_BUSY	0x80	/* Device Busy			*/
#define ATA_STAT_READY	0x40	/* Device Ready			*/
#define ATA_STAT_FAULT	0x20	/* Device Fault			*/
#define ATA_STAT_SEEK	0x10	/* Device Seek Complete		*/
#define ATA_STAT_DRQ	0x08	/* Data Request (ready)		*/
#define ATA_STAT_CORR	0x04	/* Corrected Data Error		*/
#define ATA_STAT_INDEX	0x02	/* Vendor specific		*/
#define ATA_STAT_ERR	0x01	/* Error			*/

/*
 * Device / Head Register Bits
 */
#ifndef ATA_DEVICE
#define ATA_DEVICE(x)	          ((x & 1)<<4)
#endif /* ATA_DEVICE */
#define ATA_LBA		          0xE0

#define ATA_IDENT_DEVICETYPE      0
#define ATA_IDENT_CYLINDERS       2
#define ATA_IDENT_HEADS           6
#define ATA_IDENT_SECTORS         12
#define ATA_IDENT_SERIAL          20
#define ATA_IDENT_MODEL           54
#define ATA_IDENT_CAPABILITIES    98
#define ATA_IDENT_FIELDVALID      106
#define ATA_IDENT_MAX_LBA         120
#define ATA_IDENT_COMMANDSETS     164
#define ATA_IDENT_MAX_LBA_EXT     200

#define ATA_SR_BSY                0x80
#define ATA_SR_DRDY               0x40
#define ATA_SR_DF                 0x20
#define ATA_SR_DSC                0x10
#define ATA_SR_DRQ                0x08
#define ATA_SR_CORR               0x04
#define ATA_SR_IDX                0x02
#define ATA_SR_ERR                0x01

#define ATA_ER_BBK                0x80
#define ATA_ER_UNC                0x40
#define ATA_ER_MC                 0x20
#define ATA_ER_IDNF               0x10
#define ATA_ER_MCR                0x08
#define ATA_ER_ABRT               0x04
#define ATA_ER_TK0NF              0x02
#define ATA_ER_AMNF               0x01

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC
#define ATA_CMD_SETF		  0xEF	/* Set Features			*/
#define ATA_CMD_CHK_PWR	          0xE5	/* Check Power Mode		*/

#define ATA_CMD_READ_EXT          0x24	/* Read Sectors (with retries)	with 48bit addressing */
#define ATA_CMD_WRITE_EXT	  0x34	/* Write Sectores (with retries) with 48bit addressing */
#define ATA_CMD_VRFY_EXT	  0x42	/* Read Verify	(with retries)	with 48bit addressing */

#define ATA_CMD_FLUSH             0xE7 /* Flush drive cache */
#define ATA_CMD_FLUSH_EXT         0xEA /* Flush drive cache, with 48bit addressing */

/*
 * ATAPI Commands
 */
#define ATAPI_CMD_IDENT           0xA1 /* Identify AT Atachment Packed Interface Device */
#define ATAPI_CMD_PACKET          0xA0 /* Packed Command */


#define ATAPI_CMD_INQUIRY         0x12
#define ATAPI_CMD_REQ_SENSE       0x03
#define ATAPI_CMD_READ_CAP        0x25
#define ATAPI_CMD_START_STOP      0x1B
#define ATAPI_CMD_READ_12         0xA8


#define ATA_GET_ERR()	inb(ATA_STATUS)
#define ATA_GET_STAT()	inb(ATA_STATUS)
#define ATA_OK_STAT(stat,good,bad)	(((stat)&((good)|(bad)))==(good))
#define ATA_BAD_R_STAT	(ATA_STAT_BUSY	| ATA_STAT_ERR)
#define ATA_BAD_W_STAT	(ATA_BAD_R_STAT	| ATA_STAT_FAULT)
#define ATA_BAD_STAT	(ATA_BAD_R_STAT	| ATA_STAT_DRQ)
#define ATA_DRIVE_READY	(ATA_READY_STAT	| ATA_STAT_SEEK)
#define ATA_DATA_READY	(ATA_STAT_DRQ)

#define ATA_BLOCKSIZE	512	/* bytes */
#define ATA_BLOCKSHIFT	9	/* 2 ^ ATA_BLOCKSIZESHIFT = 512 */
#define ATA_SECTORWORDS	(512 / sizeof(unsigned long))

#ifndef ATA_RESET_TIME
#define ATA_RESET_TIME	60	/* spec allows up to 31 seconds */
#endif

/* ------------------------------------------------------------------------- */

/*
 * structure returned by ATA_CMD_IDENT, as per ANSI ATA2 rev.2f spec
 */
typedef struct hd_driveid {
	unsigned short	config;		/* lots of obsolete bit flags */
	unsigned short	cyls;		/* "physical" cyls */
	unsigned short	reserved2;	/* reserved (word 2) */
	unsigned short	heads;		/* "physical" heads */
	unsigned short	track_bytes;	/* unformatted bytes per track */
	unsigned short	sector_bytes;	/* unformatted bytes per sector */
	unsigned short	sectors;	/* "physical" sectors per track */
	unsigned short	vendor0;	/* vendor unique */
	unsigned short	vendor1;	/* vendor unique */
	unsigned short	vendor2;	/* vendor unique */
	unsigned char	serial_no[20];	/* 0 = not_specified */
	unsigned short	buf_type;
	unsigned short	buf_size;	/* 512 byte increments; 0 = not_specified */
	unsigned short	ecc_bytes;	/* for r/w long cmds; 0 = not_specified */
	unsigned char	fw_rev[8];	/* 0 = not_specified */
	unsigned char	model[40];	/* 0 = not_specified */
	unsigned char	max_multsect;	/* 0=not_implemented */
	unsigned char	vendor3;	/* vendor unique */
	unsigned short	dword_io;	/* 0=not_implemented; 1=implemented */
	unsigned char	vendor4;	/* vendor unique */
	unsigned char	capability;	/* bits 0:DMA 1:LBA 2:IORDYsw 3:IORDYsup*/
	unsigned short	reserved50;	/* reserved (word 50) */
	unsigned char	vendor5;	/* vendor unique */
	unsigned char	tPIO;		/* 0=slow, 1=medium, 2=fast */
	unsigned char	vendor6;	/* vendor unique */
	unsigned char	tDMA;		/* 0=slow, 1=medium, 2=fast */
	unsigned short	field_valid;	/* bits 0:cur_ok 1:eide_ok */
	unsigned short	cur_cyls;	/* logical cylinders */
	unsigned short	cur_heads;	/* logical heads */
	unsigned short	cur_sectors;	/* logical sectors per track */
	unsigned short	cur_capacity0;	/* logical total sectors on drive */
	unsigned short	cur_capacity1;	/*  (2 words, misaligned int)     */
	unsigned char	multsect;	/* current multiple sector count */
	unsigned char	multsect_valid;	/* when (bit0==1) multsect is ok */
	unsigned int	lba_capacity;	/* total number of sectors */
	unsigned short	dma_1word;	/* single-word dma info */
	unsigned short	dma_mword;	/* multiple-word dma info */
	unsigned short  eide_pio_modes; /* bits 0:mode3 1:mode4 */
	unsigned short  eide_dma_min;	/* min mword dma cycle time (ns) */
	unsigned short  eide_dma_time;	/* recommended mword dma cycle time (ns) */
	unsigned short  eide_pio;       /* min cycle time (ns), no IORDY  */
	unsigned short  eide_pio_iordy; /* min cycle time (ns), with IORDY */
	unsigned short	words69_70[2];	/* reserved words 69-70 */
	unsigned short	words71_74[4];	/* reserved words 71-74 */
	unsigned short  queue_depth;	/*  */
	unsigned short  words76_79[4];	/* reserved words 76-79 */
	unsigned short  major_rev_num;	/*  */
	unsigned short  minor_rev_num;	/*  */
	unsigned short  command_set_1;	/* bits 0:Smart 1:Security 2:Removable 3:PM */
	unsigned short	command_set_2;	/* bits 14:Smart Enabled 13:0 zero 10:lba48 support*/
	unsigned short  cfsse;		/* command set-feature supported extensions */
	unsigned short  cfs_enable_1;	/* command set-feature enabled */
	unsigned short  cfs_enable_2;	/* command set-feature enabled */
	unsigned short  csf_default;	/* command set-feature default */
	unsigned short  dma_ultra;	/*  */
	unsigned short	word89;		/* reserved (word 89) */
	unsigned short	word90;		/* reserved (word 90) */
	unsigned short	CurAPMvalues;	/* current APM values */
	unsigned short	word92;		/* reserved (word 92) */
	unsigned short	hw_config;	/* hardware config */
	unsigned short	words94_99[6];/* reserved words 94-99 */
	/*unsigned long long  lba48_capacity; /--* 4 16bit values containing lba 48 total number of sectors */
	unsigned short	lba48_capacity[4]; /* 4 16bit values containing lba 48 total number of sectors */
	unsigned short	words104_125[22];/* reserved words 104-125 */
	unsigned short	last_lun;	/* reserved (word 126) */
	unsigned short	word127;	/* reserved (word 127) */
	unsigned short	dlf;		/* device lock function
					 * 15:9	reserved
					 * 8	security level 1:max 0:high
					 * 7:6	reserved
					 * 5	enhanced erase
					 * 4	expire
					 * 3	frozen
					 * 2	locked
					 * 1	en/disabled
					 * 0	capability
					 */
	unsigned short  csfo;		/* current set features options
					 * 15:4	reserved
					 * 3	auto reassign
					 * 2	reverting
					 * 1	read-look-ahead
					 * 0	write cache
					 */
	unsigned short	words130_155[26];/* reserved vendor words 130-155 */
	unsigned short	word156;
	unsigned short	words157_159[3];/* reserved vendor words 157-159 */
	unsigned short	words160_162[3];/* reserved words 160-162 */
	unsigned short	cf_advanced_caps;
	unsigned short	words164_255[92];/* reserved words 164-255 */
} hd_driveid_t;


/*
 * PIO Mode Configuration
 *
 * See ATA-3 (AT Attachment-3 Interface) documentation, Figure 14 / Table 21
 */

typedef struct {
	unsigned int	t_setup;	/* Setup  Time in [ns] or clocks	*/
	unsigned int	t_length;	/* Length Time in [ns] or clocks	*/
	unsigned int	t_hold;		/* Hold   Time in [ns] or clocks	*/
}
pio_config_t;

#define	IDE_MAX_PIO_MODE	4	/* max suppurted PIO mode		*/

/* ------------------------------------------------------------------------- */

#endif /* _ATA_H */
