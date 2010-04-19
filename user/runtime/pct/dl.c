
#include <stdio.h>
#include <string.h>

#define _KDBG(fmt, args...) \
printf("%s:%s:%i: " fmt, __FILE__,__FUNCTION__, __LINE__, ## args)


void *dlopen(const char *filename, int flag)
{
_KDBG("%s\n",filename);
return (void*)1;
}

char *dlerror(void)
{
_KDBG("\n");
return NULL;
}

void *dlsym(void *handle, const char *symbol)
{
	void* ret = NULL;
	if ( ! strcmp( symbol, "dat_provider_init" ) ) {
		extern int dat_provider_init();
		ret = dat_provider_init;
	} else if ( ! strcmp( symbol, "dat_provider_fini" ) ) {
		extern int dat_provider_fini();
		ret = dat_provider_fini;
	} else if ( ! strcmp( symbol, "dat_provider_init" ) ) {
		extern int dat_provider_init();
		ret = dat_provider_init;
	}
	_KDBG("handle=%p symbol=%s addr=%p\n",handle,symbol,ret);
	return ret;
}

int dlclose(void *handle)
{
_KDBG("handle=%p\n",handle);
return -1;
}

