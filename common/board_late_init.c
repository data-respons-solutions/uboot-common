#include <common.h>
#include <command.h>
#include <env.h>
#include <part.h>
#include <asm/mach-imx/hab.h>
#include <linux/libfdt.h>
#include "../include/bootsplash.h"
#include "../include/nvram.h"

int board_late_init(void)
{
	int r = 0;
	mdelay(5000);
#if defined (CONFIG_DR_NVRAM)
	r = nvram_init();
	if (r)
		printf("Failed nvram_init [%d]: %s\n", r, errno_str(r));

	if (!env_get("serial#") && nvram_get("SYS_SERIALNUMBER")) {
		if (nvram_set_env("SYS_SERIALNUMBER", "serial#"))
				printf("Failed setting serial#\n");
	}
	printf("Serial number: %s\n", env_get("serial#"));

#endif
#if defined(CONFIG_DR_NVRAM_BOOTSPLASH)
	printf("Enabling bootsplash...\n");
	if ((r = bootsplash_load()))
		printf("Failed loading bootsplash [%d]: %s\n", r, errno_str(r));
#endif

	return 0;
}
