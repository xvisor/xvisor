#ifndef _GENERIC_UNALIGNED_H
#define _GENERIC_UNALIGNED_H

#include <libs/unaligned/le_byteshift.h>
#include <libs/unaligned/be_byteshift.h>
#include <libs/unaligned/generic.h>

/*
 * Select endianness
 */
#if defined(CONFIG_CPU_LE)
#define get_unaligned	__get_unaligned_le
#define put_unaligned	__put_unaligned_le
#elif defined(CONFIG_CPU_BE)
#define get_unaligned	__get_unaligned_be
#define put_unaligned	__put_unaligned_be
#else
#error invalid endian
#endif

#endif
