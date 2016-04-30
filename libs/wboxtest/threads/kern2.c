/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file kern2.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief kern2 test implementation
 *
 * This source has been largely adapted from Atomthreads Sources:
 * <atomthreads_source>/tests/kern2.c
 *
 * For more info visit: http://atomthreads.com
 */

#include <vmm_error.h>
#include <vmm_delay.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"kern2 test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define	MODULE_INIT			kern2_init
#define	MODULE_EXIT			kern2_exit

enum kern2_delay_type {
	KERN2_YIELD=1,
	KERN2_UDELAY=2,
	KERN2_MDELAY=3,
	KERN2_SDELAY=4,
	KERN2_USLEEP=5,
	KERN2_MSLEEP=6,
	KERN2_SSLEEP=7,
};

static int kern2_do_test(struct vmm_chardev *cdev,
			 unsigned long darg,
			 enum kern2_delay_type dtype)
{
	int failures;
	u8 one = 1;
	u8 two = 2;
	u8 three = 3;
	u8 four = 4;
	u8 five = 5;
	u8 six = 6;
	u8 seven = 7;
	u8 eight = 8;
	u8 nine = 9;
	u8 ten = 10;
	u16 eleven = 11;
	u16 twelve = 12;
	u16 thirteen = 13;
	u16 fourteen = 14;
	u16 fifteen = 15;
	u16 sixteen = 16;
	u16 seventeen = 17;
	u16 eighteen = 18;
	u16 nineteen = 19;
	u16 twenty = 20;
	u32 twentyone = 21;
	u32 twentytwo = 22;
	u32 twentythree = 23;
	u32 twentyfour = 24;
	u32 twentyfive = 25;
	u32 twentysix = 26;
	u32 twentyseven = 27;
	u32 twentyeight = 28;
	u32 twentynine = 29;
	u32 thirty = 30;
	u64 thirtyone = 31;
	u64 thirtytwo = 32;
	u64 thirtythree = 33;
	u64 thirtyfour = 34;
	u64 thirtyfive = 35;
	u64 thirtysix = 36;
	u64 thirtyseven = 37;
	u64 thirtyeight = 38;
	u64 thirtynine = 39;
	u64 fourty = 40;

	/* Default to zero failures */
	failures = 0;

	/* some delay for scheduler to get invoked */
	switch (dtype) {
	case KERN2_YIELD:
		while (darg) {
			vmm_scheduler_yield();
			darg--;
		}
		break;
	case KERN2_UDELAY:
		vmm_udelay(darg);
		break;
	case KERN2_MDELAY:
		vmm_mdelay(darg);
		break;
	case KERN2_SDELAY:
		vmm_sdelay(darg);
		break;
	case KERN2_USLEEP:
		vmm_usleep(darg);
		break;
	case KERN2_MSLEEP:
		vmm_msleep(darg);
		break;
	case KERN2_SSLEEP:
		vmm_ssleep(darg);
		break;
	};

	/* Check all variables contain expected values */
	if (one != 1) {
		vmm_cprintf(cdev, "1(%d)\n", (int)one);
		failures++;
	}
	if (two != 2) {
		vmm_cprintf(cdev, "2(%d)\n", (int)two);
		failures++;
	}
	if (three != 3) {
		vmm_cprintf(cdev, "3(%d)\n", (int)three);
		failures++;
	}
	if (four != 4) {
		vmm_cprintf(cdev, "4(%d)\n", (int)four);
		failures++;
	}
	if (five != 5) {
		vmm_cprintf(cdev, "5(%d)\n", (int)five);
		failures++;
	}
	if (six != 6) {
		vmm_cprintf(cdev, "6(%d)\n", (int)six);
		failures++;
	}
	if (seven != 7) {
		vmm_cprintf(cdev, "7(%d)\n", (int)seven);
		failures++;
	}
	if (eight != 8) {
		vmm_cprintf(cdev, "8(%d)\n", (int)eight);
		failures++;
	}
	if (nine != 9) {
		vmm_cprintf(cdev, "9(%d)\n", (int)nine);
		failures++;
	}
	if (ten != 10) {
		vmm_cprintf(cdev, "10(%d)\n", (int)ten);
		failures++;
	}
	if (eleven != 11) {
		vmm_cprintf(cdev, "11(%d)\n", (int)eleven);
		failures++;
	}
	if (twelve != 12) {
		vmm_cprintf(cdev, "12(%d)\n", (int)twelve);
		failures++;
	}
	if (thirteen != 13) {
		vmm_cprintf(cdev, "13(%d)\n", (int)thirteen);
		failures++;
	}
	if (fourteen != 14) {
		vmm_cprintf(cdev, "14(%d)\n", (int)fourteen);
		failures++;
	}
	if (fifteen != 15) {
		vmm_cprintf(cdev, "15(%d)\n", (int)fifteen);
		failures++;
	}
	if (sixteen != 16) {
		vmm_cprintf(cdev, "16(%d)\n", (int)sixteen);
		failures++;
	}
	if (seventeen != 17) {
		vmm_cprintf(cdev, "17(%d)\n", (int)seventeen);
		failures++;
	}
	if (eighteen != 18) {
		vmm_cprintf(cdev, "18(%d)\n", (int)eighteen);
		failures++;
	}
	if (nineteen != 19) {
		vmm_cprintf(cdev, "19(%d)\n", (int)nineteen);
		failures++;
	}
	if (twenty != 20) {
		vmm_cprintf(cdev, "20(%d)\n", (int)twenty);
		failures++;
	}
	if (twentyone != 21) {
		vmm_cprintf(cdev, "21(%d)\n", (int)twentyone);
		failures++;
	}
	if (twentytwo != 22) {
		vmm_cprintf(cdev, "22(%d)\n", (int)twentytwo);
		failures++;
	}
	if (twentythree != 23) {
		vmm_cprintf(cdev, "23(%d)\n", (int)twentythree);
		failures++;
	}
	if (twentyfour != 24) {
		vmm_cprintf(cdev, "24(%d)\n", (int)twentyfour);
		failures++;
	}
	if (twentyfive != 25) {
		vmm_cprintf(cdev, "25(%d)\n", (int)twentyfive);
		failures++;
	}
	if (twentysix != 26) {
		vmm_cprintf(cdev, "26(%d)\n", (int)twentysix);
		failures++;
	}
	if (twentyseven != 27) {
		vmm_cprintf(cdev, "27(%d)\n", (int)twentyseven);
		failures++;
	}
	if (twentyeight != 28) {
		vmm_cprintf(cdev, "28(%d)\n", (int)twentyeight);
		failures++;
	}
	if (twentynine != 29) {
		vmm_cprintf(cdev, "29(%d)\n", (int)twentynine);
		failures++;
	}
	if (thirty != 30) {
		vmm_cprintf(cdev, "30(%d)\n", (int)thirty);
		failures++;
	}
	if (thirtyone != 31) {
		vmm_cprintf(cdev, "31(%d)\n", (int)thirtyone);
		failures++;
	}
	if (thirtytwo != 32) {
		vmm_cprintf(cdev, "32(%d)\n", (int)thirtytwo);
		failures++;
	}
	if (thirtythree != 33) {
		vmm_cprintf(cdev, "33(%d)\n", (int)thirtythree);
		failures++;
	}
	if (thirtyfour != 34) {
		vmm_cprintf(cdev, "34(%d)\n", (int)thirtyfour);
		failures++;
	}
	if (thirtyfive != 35) {
		vmm_cprintf(cdev, "35(%d)\n", (int)thirtyfive);
		failures++;
	}
	if (thirtysix != 36) {
		vmm_cprintf(cdev, "36(%d)\n", (int)thirtysix);
		failures++;
	}
	if (thirtyseven != 37) {
		vmm_cprintf(cdev, "37(%d)\n", (int)thirtyseven);
		failures++;
	}
	if (thirtyeight != 38) {
		vmm_cprintf(cdev, "38(%d)\n", (int)thirtyeight);
		failures++;
	}
	if (thirtynine != 39) {
		vmm_cprintf(cdev, "39(%d)\n", (int)thirtynine);
		failures++;
	}
	if (fourty != 40) {
		vmm_cprintf(cdev, "40(%d)\n", (int)fourty);
		failures++;
	}

	return (failures) ? VMM_EFAIL : VMM_OK;
}

static int kern2_run(struct wboxtest *test, struct vmm_chardev *cdev)
{
	int rc;

	rc = kern2_do_test(cdev, 10, KERN2_YIELD);
	if (rc) {
		vmm_cprintf(cdev, "kern2 yield() failed\n");
		return rc;
	};

	rc = kern2_do_test(cdev, 1000000, KERN2_UDELAY);
	if (rc) {
		vmm_cprintf(cdev, "kern2 udelay() failed\n");
		return rc;
	};

	rc = kern2_do_test(cdev, 1000, KERN2_MDELAY);
	if (rc) {
		vmm_cprintf(cdev, "kern2 mdelay() failed\n");
		return rc;
	};

	rc = kern2_do_test(cdev, 1, KERN2_SDELAY);
	if (rc) {
		vmm_cprintf(cdev, "kern2 sdelay() failed\n");
		return rc;
	};

	rc = kern2_do_test(cdev, 1000000, KERN2_USLEEP);
	if (rc) {
		vmm_cprintf(cdev, "kern2 usleep() failed\n");
		return rc;
	};

	rc = kern2_do_test(cdev, 1000, KERN2_MSLEEP);
	if (rc) {
		vmm_cprintf(cdev, "kern2 msleep() failed\n");
		return rc;
	};

	rc = kern2_do_test(cdev, 1, KERN2_SSLEEP);
	if (rc) {
		vmm_cprintf(cdev, "kern2 ssleep() failed\n");
		return rc;
	};

	return 0;
}

static struct wboxtest kern2 = {
	.name = "kern2",
	.run = kern2_run,
};

static int __init kern2_init(void)
{
	return wboxtest_register("threads", &kern2);
}

static void __exit kern2_exit(void)
{
	wboxtest_unregister(&kern2);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
