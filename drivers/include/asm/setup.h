#ifndef __ASM_SETUP_H
#define __ASM_SETUP_H

#include <asm/cache.h>

#ifdef CONFIG_X86
#define COMMAND_LINE_SIZE 2048
#else
#define COMMAND_LINE_SIZE 1024
#endif

#endif  /* __ASM_SETUP_H */
