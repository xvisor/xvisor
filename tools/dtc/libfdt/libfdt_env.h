#ifndef _LIBFDT_ENV_H
#define _LIBFDT_ENV_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define _B(n)	((((unsigned long long)x) >> (8*n)) & 0xFF)

static inline uint32_t fdt32_to_cpu_e(uint32_t x, int32_t is_le)
{
	uint32_t rx = 0x0;
	uint8_t * xp = (uint8_t *)&rx;
	if (is_le) {
		xp[0] = _B(0);
		xp[1] = _B(1);
		xp[2] = _B(2);
		xp[3] = _B(3);
	} else {
		xp[0] = _B(3);
		xp[1] = _B(2);
		xp[2] = _B(1);
		xp[3] = _B(0);
	}
	return rx;
}
#define cpu_to_fdt32_e(x, is_le) fdt32_to_cpu_e(x, is_le)

static inline uint64_t fdt64_to_cpu_e(uint64_t x, int32_t is_le)
{
	uint64_t rx = 0x0;
	uint8_t * xp = (uint8_t *)&rx;
	if (is_le) {
		xp[0] = _B(0);
		xp[1] = _B(1);
		xp[2] = _B(2);
		xp[3] = _B(3);
		xp[4] = _B(4);
		xp[5] = _B(5);
		xp[6] = _B(6);
		xp[7] = _B(7);
	} else {
		xp[0] = _B(7);
		xp[1] = _B(6);
		xp[2] = _B(5);
		xp[3] = _B(4);
		xp[4] = _B(3);
		xp[5] = _B(2);
		xp[6] = _B(1);
		xp[7] = _B(0);
	}
	return rx;
}
#define cpu_to_fdt64_e(x, is_le) fdt64_to_cpu_e(x, is_le)

#define fdt32_to_cpu(x) fdt32_to_cpu_e(x, 0)
#define cpu_to_fdt32(x) fdt32_to_cpu_e(x, 0)
#define cpu_to_fdt64(x) fdt64_to_cpu_e(x, 0)
#define fdt64_to_cpu(x) fdt64_to_cpu_e(x, 0)

#undef _B

#endif /* _LIBFDT_ENV_H */
