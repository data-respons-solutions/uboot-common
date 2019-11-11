#include <vsprintf.h>
#include "../include/bootcount.h"
#include "../include/nvram.h"

#if !(ULONG_MAX == UINT32_MAX)
#error "Unsupported ulong size (ulong != uint32_t)"
#endif

int nvram_bootcount_store(uint32_t val)
{
	char* str = simple_itoa(val);
	return nvram_set("sys_bootcount", str);
}

uint32_t nvram_bootcount_load(void)
{
	ulong count = 0;
	char* str = nvram_get("sys_bootcount");
	if (!str) {
		return 0;
	}
	if (str2long(str, &count)) {
		return count;
	}
	return 0;
}
