#ifndef _LINUX_SEQ_FILE_H
#define _LINUX_SEQ_FILE_H

#include <vmm_chardev.h>
#include <vmm_stdio.h>

#define seq_file		vmm_chardev

#define seq_printf(s, msg...)	vmm_cprintf(s, msg)

#define seq_putc(s, ch)		vmm_cputc(s, ch)

#define seq_puts(s, str)	vmm_cputs(s, str)

#endif
