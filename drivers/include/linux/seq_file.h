#ifndef _LINUX_SEQ_FILE_H
#define _LINUX_SEQ_FILE_H

#include <vmm_chardev.h>
#include <vmm_stdio.h>

#define seq_file		vmm_chardev

#define seq_printf(s, msg...)	vmm_cprintf(s, msg)

#endif
