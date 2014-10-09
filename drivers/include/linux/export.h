#ifndef _LINUX_EXPORT_H
#define _LINUX_EXPORT_H

#include <vmm_modules.h>

#define EXPORT_SYMBOL(sym)		VMM_EXPORT_SYMBOL(sym)
#define EXPORT_SYMBOL_GPL(sym)		VMM_EXPORT_SYMBOL_GPL(sym)

#endif
