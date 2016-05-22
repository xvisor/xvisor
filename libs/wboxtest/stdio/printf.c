/**
 * Copyright (c) 2016 Open Wide
 *               2016 Institut de Recherche Technologique SystemX
 *
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
 * @file printf.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief printf display tests
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"printf test"
#define MODULE_AUTHOR			"Jean Guyomarc'h"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define	MODULE_INIT			wb_printf_init
#define	MODULE_EXIT			wb_printf_exit

static int wb_printf_run(struct wboxtest *test, struct vmm_chardev *cdev,
			 u32 test_hcpu)
{
        char buf[1024];
        int rc = VMM_OK;
        const virtual_addr_t deadbeef = 0xdeadbeef;
        const physical_addr_t babe = 0xbabe;
        const virtual_size_t size = 42;
        const s64 var = 777;
        const u64 feeddeadbabe = 0xfeeddeadbabeLL;
	bool colors;

#ifdef CONFIG_LOG_ANSI_COLORS
	colors = TRUE;
#else
	colors = FALSE;
#endif

#define _TEST(expect_, fmt_, ...) \
        do { \
                vmm_snprintf(buf, sizeof(buf), fmt_, ## __VA_ARGS__); \
                vmm_cprintf(cdev, "Expecting [%s], wrote [%s]... ", \
                            expect_, buf); \
                if (strcmp(buf, expect_) != 0) { \
                        rc = VMM_EFAIL; \
                        vmm_cprintf(cdev, "FAIL!\n"); \
                } else { \
                        vmm_cprintf(cdev, "ok\n"); \
                } \
        } while (0)
        /*===================================================================*/

        _TEST("1024 = 2^10", "%d = %i^%s", 1024, 2, "10");
        _TEST("0xbad = 0XBAD", "0x%x = 0X%X", 0xbad, 2989); /* 0xbad = 2989 */
        _TEST("1 + 1 + 1 + 777 = 780",
              "%"PRId8" + %"PRId16" + %"PRId32" + %"PRId64" = %d",
              (s8)1, (s16)1, (s32)1, var, 780);
        _TEST("1 + 1 + 1 + 777 = 780",
              "%"PRIi8" + %"PRIi16" + %"PRIi32" + %"PRIi64" = %i",
              (s8)1, (s16)1, (s32)1, var, 780);
        _TEST("1 + 1 + 1 + 777 = 780",
              "%"PRIu8" + %"PRIu16" + %"PRIu32" + %"PRIu64" = %u",
              (u8)1, (u16)1, (u32)1, var, 780);
        _TEST("0xfeeddeadbabe = 280297596631742",
              "0x%"PRIx64" = %"PRIu64, feeddeadbabe, feeddeadbabe);
        if (sizeof(void *) == sizeof(u32)) {
                _TEST("0xDEADBEEF", "0x%"PRIADDR, deadbeef);
                _TEST("0x0000BABE", "0x%"PRIADDR, babe);
        } else if (sizeof(void *) == sizeof(u64)) {
                _TEST("0x00000000DEADBEEF", "0x%"PRIADDR, deadbeef);
                _TEST("0x000000000000BABE", "0x%"PRIADDR, babe);
        }
        _TEST("42 % 2 = 0", "%"PRISIZE" %% %u = %i", size, 2, 0);
        _TEST("Xvisor", "%c%c%c%c%c%c", 'X', 'v', 'i', 's', 'o', 'r');

        /*===================================================================*/
#undef _TEST

	/*
	 * Show the color capabilities
	 */
	vmm_printf("\nTrying out vmm_lprintf(): "
		   "log level is %ld, colors are %s.\n",
		   vmm_stdio_loglevel(), colors ? "enabled" : "disabled");
	vmm_linfo(NULL, "This is an information message\n");
	vmm_lnotice(NULL, "This is a notice message\n");
	vmm_lwarning(NULL, "This is a warning message\n");
	vmm_lerror(NULL, "This is an error message\n");
	vmm_lcritical(NULL, "This is a critical message\n");
	vmm_lalert(NULL, "This is an alert message\n");
	vmm_lemergency(NULL, "This is an emergency message\n");
	vmm_printf("\n");

        return rc;
}

static struct wboxtest wb_printf = {
        .name = "printf",
        .run = wb_printf_run,
};

static int __init wb_printf_init(void)
{
        return wboxtest_register("stdio", &wb_printf);
}

static void __exit wb_printf_exit(void)
{
        wboxtest_unregister(&wb_printf);
}

VMM_DECLARE_MODULE(MODULE_DESC,
                   MODULE_AUTHOR,
                   MODULE_LICENSE,
                   MODULE_IPRIORITY,
                   MODULE_INIT,
                   MODULE_EXIT);
