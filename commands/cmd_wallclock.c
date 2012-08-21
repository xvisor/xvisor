/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cmd_wallclock.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of wallclock command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <vmm_wallclock.h>
#include <stringlib.h>

#define MODULE_DESC			"Command wallclock"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_wallclock_init
#define	MODULE_EXIT			cmd_wallclock_exit

void cmd_wallclock_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   wallclock help\n");
	vmm_cprintf(cdev, "   wallclock get_time\n");
	vmm_cprintf(cdev, "   wallclock set_time <hour>:<min>:<sec> "
			      "<day> <month> <year> [+/-<tz_hour>:<tz_min]\n");
	vmm_cprintf(cdev, "   wallclock get_timezone\n");
	vmm_cprintf(cdev, "   wallclock set_timezone +/-<tz_hour>:<tz_min\n");
	vmm_cprintf(cdev, "Note:\n");
	vmm_cprintf(cdev, "   <hour>    = any value between 0..23\n");
	vmm_cprintf(cdev, "   <minute>  = any value between 0..59\n");
	vmm_cprintf(cdev, "   <second>  = any value between 0..59\n");
	vmm_cprintf(cdev, "   <day>     = any value between 0..31\n");
	vmm_cprintf(cdev, "   <month>   = Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|"
						"Sep|Oct|Nov|Dec\n");
	vmm_cprintf(cdev, "   <year>    = any value greater than 1970\n");
	vmm_cprintf(cdev, "   <tz_hour> = timezone hour\n");
	vmm_cprintf(cdev, "   <tz_min>  = timezone minutes\n");
}

int cmd_wallclock_get_time(struct vmm_chardev *cdev)
{
	int rc;
	struct vmm_timeinfo ti;
	struct vmm_timeval tv;
	struct vmm_timezone tz;

	rc = vmm_wallclock_get_timeofday(&tv, &tz);
	if (rc) {
		vmm_cprintf(cdev, "Error: get_time failed\n");
		return rc;
	}

	vmm_wallclock_mkinfo(tv.tv_sec, 0, &ti);

	switch (ti.tm_wday) {
	case 0:
		vmm_cprintf(cdev, "%s ", "Sun");
		break;
	case 1:
		vmm_cprintf(cdev, "%s ", "Mon");
		break;
	case 2:
		vmm_cprintf(cdev, "%s ", "Tue");
		break;
	case 3:
		vmm_cprintf(cdev, "%s ", "Wed");
		break;
	case 4:
		vmm_cprintf(cdev, "%s ", "Thu");
		break;
	case 5:
		vmm_cprintf(cdev, "%s ", "Fri");
		break;
	case 6:
		vmm_cprintf(cdev, "%s ", "Sat");
		break;
	default:
		vmm_cprintf(cdev, "Error: Invalid day of week\n");
	};

	switch (ti.tm_mon) {
	case 0:
		vmm_cprintf(cdev, "%s ", "Jan");
		break;
	case 1:
		vmm_cprintf(cdev, "%s ", "Feb");
		break;
	case 2:
		vmm_cprintf(cdev, "%s ", "Mar");
		break;
	case 3:
		vmm_cprintf(cdev, "%s ", "Apr");
		break;
	case 4:
		vmm_cprintf(cdev, "%s ", "May");
		break;
	case 5:
		vmm_cprintf(cdev, "%s ", "Jun");
		break;
	case 6:
		vmm_cprintf(cdev, "%s ", "Jul");
		break;
	case 7:
		vmm_cprintf(cdev, "%s ", "Aug");
		break;
	case 8:
		vmm_cprintf(cdev, "%s ", "Sep");
		break;
	case 9:
		vmm_cprintf(cdev, "%s ", "Oct");
		break;
	case 10:
		vmm_cprintf(cdev, "%s ", "Nov");
		break;
	case 11:
		vmm_cprintf(cdev, "%s ", "Dec");
		break;
	default:
		vmm_cprintf(cdev, "Error: Invalid month\n");
	};

	vmm_cprintf(cdev, "%2d %d:%d:%d ", ti.tm_mday, 
					ti.tm_hour, ti.tm_min, ti.tm_sec);
	if (tz.tz_minuteswest == 0) {
		vmm_cprintf(cdev, "UTC ");
	} else if (tz.tz_minuteswest < 0) {
		tz.tz_minuteswest *= -1;
		vmm_cprintf(cdev, "UTC-%d:%d ", tz.tz_minuteswest / 60, 
						tz.tz_minuteswest % 60);
	} else {
		vmm_cprintf(cdev, "UTC+%d:%d ", tz.tz_minuteswest / 60, 
						tz.tz_minuteswest % 60);
	}

	vmm_cprintf(cdev, "%d", ti.tm_year + 1900);

	vmm_cprintf(cdev, "\n");

	return VMM_OK;
}

int cmd_wallclock_set_time(struct vmm_chardev *cdev, 
				int targc, char **targv)
{
	int rc;
	char * s;
	struct vmm_timeinfo ti;
	struct vmm_timeval tv;
	struct vmm_timezone tz;

	if (targc > 4) {
		s = targv[4];
		rc = 0;
		tz.tz_minuteswest = 0;
		tz.tz_dsttime = 0;
		if (*s == '-' || *s == '+') {
			s++;
		}
		while (*s) {
			if (*s == ':') {
				rc++;
			} else if ('0' <= *s && *s <= '9') {
				switch(rc) {
				case 0:
					tz.tz_dsttime = 
					tz.tz_dsttime * 10 + (*s - '0');
					break;
				case 1:
					tz.tz_minuteswest = 
					tz.tz_minuteswest * 10 + (*s - '0');
					break;
				default:
					break;
				};
			}
			s++;
		}
		rc = 0;
		tz.tz_minuteswest += tz.tz_dsttime * 60;
		tz.tz_dsttime = 0;
		s = targv[4];
		if (*s == '-') {
			tz.tz_minuteswest *= -1;
		}

		if ((rc = vmm_wallclock_set_timezone(&tz))) {
			vmm_cprintf(cdev, "Error: set_timezone failed\n");
			return rc;
		}
	}

	s = targv[0];
	rc = 0;
	ti.tm_hour = 0;
	ti.tm_min = 0;
	ti.tm_sec = 0;
	while (*s) {
		if (*s == ':') {
			rc++;
		} else if ('0' <= *s && *s <= '9') {
			switch(rc) {
			case 0:
				ti.tm_hour = ti.tm_hour * 10 + (*s - '0');
				break;
			case 1:
				ti.tm_min = ti.tm_min * 10 + (*s - '0');
				break;
			case 2:
				ti.tm_sec = ti.tm_sec * 10 + (*s - '0');
				break;
			default:
				break;
			};
		}
		s++;
	}
	rc = 0;
	ti.tm_mday = str2int(targv[1], 10);
	str2lower(targv[2]);
	if (strcmp(targv[2], "jan") == 0) {
		ti.tm_mon = 0;
	} else if (strcmp(targv[2], "feb") == 0) {
		ti.tm_mon = 1;
	} else if (strcmp(targv[2], "mar") == 0) {
		ti.tm_mon = 2;
	} else if (strcmp(targv[2], "apr") == 0) {
		ti.tm_mon = 3;
	} else if (strcmp(targv[2], "may") == 0) {
		ti.tm_mon = 4;
	} else if (strcmp(targv[2], "jun") == 0) {
		ti.tm_mon = 5;
	} else if (strcmp(targv[2], "jul") == 0) {
		ti.tm_mon = 6;
	} else if (strcmp(targv[2], "aug") == 0) {
		ti.tm_mon = 7;
	} else if (strcmp(targv[2], "sep") == 0) {
		ti.tm_mon = 8;
	} else if (strcmp(targv[2], "oct") == 0) {
		ti.tm_mon = 9;
	} else if (strcmp(targv[2], "nov") == 0) {
		ti.tm_mon = 10;
	} else if (strcmp(targv[2], "dec") == 0) {
		ti.tm_mon = 11;
	} else {
		ti.tm_mon = str2int(targv[2], 10);
	}
	ti.tm_year = str2int(targv[3], 10) - 1900;

	tv.tv_sec = vmm_wallclock_mktime(ti.tm_year + 1900, 
					 ti.tm_mon + 1, 
					 ti.tm_mday, 
					 ti.tm_hour, 
					 ti.tm_min, 
					 ti.tm_sec);
	tv.tv_nsec = 0;

	if ((rc = vmm_wallclock_set_local_time(&tv))) {
		vmm_cprintf(cdev, "Error: set_local_time failed\n");
		return rc;
	}

	return VMM_OK;
}

int cmd_wallclock_get_timezone(struct vmm_chardev *cdev)
{
	int rc;
	struct vmm_timezone tz;

	if ((rc = vmm_wallclock_get_timezone(&tz))) {
		vmm_cprintf(cdev, "Error: get_timezone failed\n");
		return rc;
	}

	if (tz.tz_minuteswest == 0) {
		vmm_cprintf(cdev, "UTC\n");
	} else if (tz.tz_minuteswest < 0) {
		tz.tz_minuteswest *= -1;
		vmm_cprintf(cdev, "UTC-%d:%d\n", tz.tz_minuteswest / 60, 
						 tz.tz_minuteswest % 60);
	} else {
		vmm_cprintf(cdev, "UTC+%d:%d\n", tz.tz_minuteswest / 60, 
						 tz.tz_minuteswest % 60);
	}

	return VMM_OK;
}

int cmd_wallclock_set_timezone(struct vmm_chardev *cdev, char *tzstr)
{
	int rc;
	char * s;
	struct vmm_timezone tz;

	s = tzstr;
	rc = 0;
	tz.tz_minuteswest = 0;
	tz.tz_dsttime = 0;
	if (*s == '-' || *s == '+') {
		s++;
	}
	while (*s) {
		if (*s == ':') {
			rc++;
		} else if ('0' <= *s && *s <= '9') {
			switch(rc) {
			case 0:
				tz.tz_dsttime = 
				tz.tz_dsttime * 10 + (*s - '0');
				break;
			case 1:
				tz.tz_minuteswest = 
				tz.tz_minuteswest * 10 + (*s - '0');
				break;
			default:
				break;
			};
		}
		s++;
	}
	rc = 0;
	tz.tz_minuteswest += tz.tz_dsttime * 60;
	tz.tz_dsttime = 0;
	s = tzstr;
	if (*s == '-') {
		tz.tz_minuteswest *= -1;
	}

	if ((rc = vmm_wallclock_set_timezone(&tz))) {
		vmm_cprintf(cdev, "Error: set_timezone failed\n");
		return rc;
	}

	return VMM_OK;
}

int cmd_wallclock_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_wallclock_usage(cdev);
			return VMM_OK;
		}
	}
	if (argc < 2) {
		cmd_wallclock_usage(cdev);
		return VMM_EFAIL;
	}
	if (strcmp(argv[1], "get_time") == 0) {
		return cmd_wallclock_get_time(cdev);
	} else if ((strcmp(argv[1], "set_time") == 0) && argc >= 6) {
		return cmd_wallclock_set_time(cdev, argc - 2, &argv[2]);
	} else if (strcmp(argv[1], "get_timezone") == 0) {
		return cmd_wallclock_get_timezone(cdev);
	} else if ((strcmp(argv[1], "set_timezone") == 0) && argc == 3) {
		return cmd_wallclock_set_timezone(cdev, argv[2]);
	}
	cmd_wallclock_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_wallclock = {
	.name = "wallclock",
	.desc = "wall-clock commands",
	.usage = cmd_wallclock_usage,
	.exec = cmd_wallclock_exec,
};

static int __init cmd_wallclock_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_wallclock);
}

static void __exit cmd_wallclock_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_wallclock);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
