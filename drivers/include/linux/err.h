#ifndef _LINUX_ERR_H
#define _LINUX_ERR_H

#include <vmm_compiler.h>
#include <linux/printk.h>
#include <asm/errno.h>

/* Indirect macros required for expanded argument pasting, eg. __LINE__. */
#define ___PASTE(a,b) a##b
#define __PASTE(a,b) ___PASTE(a,b)

#define __must_check		__mustcheck
#define __force

#define MAX_ERRNO		VMM_MAX_ERRNO

#define IS_ERR_VALUE(x) 	VMM_IS_ERR_VALUE(x)

#define ERR_PTR(error)		VMM_ERR_PTR(error)

#define PTR_ERR(ptr)		VMM_PTR_ERR(ptr)

#define IS_ERR(ptr)		VMM_IS_ERR(ptr)

#define IS_ERR_OR_NULL(ptr)	VMM_IS_ERR_OR_NULL(ptr)

#define ERR_CAST(ptr)		VMM_ERR_CAST(ptr)

#define PTR_RET(ptr)		VMM_PTR_ERR(ptr)

#endif /* _LINUX_ERR_H */
