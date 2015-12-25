#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include "openconf.h"

#define P(name,type,arg)	type (*name ## _p) arg
#include "openconf_proto.h"
#undef P

void openconf_load(void)
{
	void *handle;
	char *error;

	handle = dlopen("./libopenconf.so", RTLD_LAZY);
	if (!handle) {
		handle = dlopen("/usr/lib/libopenconf.so", RTLD_LAZY);
		if (!handle) {
			fprintf(stderr, "%s\n", dlerror());
			exit(1);
		}
	}

#define P(name,type,arg)			\
{						\
	name ## _p = dlsym(handle, #name);	\
        if ((error = dlerror()))  {		\
                fprintf(stderr, "%s\n", error);	\
		exit(1);			\
	}					\
}
#include "openconf_proto.h"
#undef P
}
