#include <common.h>
#include <command.h>
#include <asm/mach-imx/hab.h>
#include <linux/libfdt.h>
#include "../include/bootsplash.h"

static void select_fdt(void)
{
	void* fdt_addr = (void*) env_get_hex("fdt_addr", 0);
	int r = 0;

	if (!fdt_addr) {
		printf("%s: env \"fdt_addr\" not defined -- ignore fdt\n", __func__);
		return;
	}

	r = fdt_check_header(fdt_addr);
	if (!r) {
		printf("%s: valid fdt found in fdt_addr 0x%p\n", __func__, fdt_addr);
		return;
	}
#if 0
/*
 * Linux wasn't able to boot with devicetree from uboot.
 */
#if defined(CONFIG_OF_CONTROL)
	if (gd->fdt_blob) {
		r = fdt_check_header(gd->fdt_blob);
		if (!r) {
			printf("%s: valid fdt found in uboot 0x%p -- copy to fdt_addr 0x%p\n", __func__, gd->fdt_blob, fdt_addr);
			ulong fdt_high = env_get_hex("fdt_high", ULONG_MAX);
			r = fdt_move(gd->fdt_blob, fdt_addr, fdt_high - (ulong) fdt_addr);
			if (r) {
				printf("%s: fdt_move [%d]: %s\n", __func__, r, fdt_strerror(r));
			}
			else {
				return;
			}
		}
	}
#endif
#endif
	printf("%s: no fdt found in platform\n", __func__);
}

#if defined(CONFIG_CMD_USB)
static int start_usb(void)
{
	unsigned long ticks = 0;
	int rep = 0;

	char* const start[] = {"usb", "start"};
	if (cmd_process(0, ARRAY_SIZE(start), start, &rep, &ticks)) {
		return -EIO;
	}

	return 0;
}
#endif

int board_late_init(void)
{
	int r = 0;

#if defined(CONFIG_DR_NVRAM_BOOTSPLASH)
	printf("Enabling bootsplash...\n");
	if ((r = bootsplash_load())) {
		printf("Failed loading bootsplash [%d]: %s\n", -r, errno_str(-r));
	}
#endif

	select_fdt();

#if defined(CONFIG_CMD_USB)
	start_usb();
#endif

#if defined(CONFIG_SECURE_BOOT)
	if (imx_hab_is_enabled()) {
		printf("HAB enabled, setting up secure bootscript\n");
		env_set("bootscript", BOOTSCRIPT_SECURE);
		env_set("zimage", DEFAULT_ZIMAGE_SECURE);
		env_set("initrd_file", DEFAULT_INITRD_SECURE);
	}
	else
	{
		printf("HAB disabled, using regular bootscript\n");
	}

	return 0;
#endif
}
