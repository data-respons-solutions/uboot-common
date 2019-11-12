#include <common.h>
#include <command.h>
#include <asm/mach-imx/hab.h>
#include <linux/libfdt.h>
#include "../include/bootsplash.h"
#include "../include/bootcount.h"
#include "../include/nvram.h"

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

#if defined(CONFIG_DR_NVRAM_BOOTCOUNT)
static int increment_bootcounter(void)
{
	ulong counter = nvram_get_ulong("sys_bootcount", 10, 0);
	return nvram_set_ulong("sys_bootcount", counter +1);
}
#endif

#if defined(CONFIG_DR_NVRAM)
static int nvram_init_env(void)
{
	const char* part = nvram_get("sys_boot_part");
	if (!part) {
		printf("%s: sys_boot_part: reset to default: \"%s\"\n", __func__, DEFAULT_MMC_PART);
		if (nvram_set("sys_boot_part", DEFAULT_MMC_PART)) {
			return -EFAULT;
		}
	}

	if (nvram_set_env("sys_boot_part", "mmcpart")) {
		return -EFAULT;
	}

	return 0;
}
#endif

#if defined(CONFIG_DR_NVRAM_BOOT_SWAP)
/*
 * Modes detected during boot:
 *
 * x = don't care
 *
 * mode        | _verified |  _swap  |     _start       |
 * normal      |      x    |  _part  |       x          |
 * init swap   |    true   |  !_part |      NULL        |
 * swapping    |    false  |  !_part |  < bootcount +3  |
 * rollback    |    false  |  !_part |    bootcount +3  |
 * verified    |    true   |  !_part |     !NULL        |
 *
 */

static int nvram_boot_swap(void)
{
	const char* verified = nvram_get("sys_boot_verified");
	if (!verified) {
		printf("%s: sys_boot_verified: reset to default: \"true\"\n", __func__);
		if (nvram_set("sys_boot_verified", "true")) {
			return -EFAULT;
		}
	}
	const char* swap = nvram_get("sys_boot_swap");
	if (!swap) {
		printf("%s: sys_boot_swap: reset to default: \"%s\"\n", __func__, nvram_get("sys_boot_part"));
		if (nvram_set("sys_boot_swap", nvram_get("sys_boot_part"))) {
			return -EFAULT;
		}
	}

	if (!strcmp(nvram_get("sys_boot_part"), nvram_get("sys_boot_swap"))) {
		/* mode normal */
		printf("%s: verified state\n", __func__);
		if (strcmp(nvram_get("sys_boot_verified"), "true")) {
			printf("%s: setting sys_boot_verfied to true\n", __func__);
			if (nvram_set("sys_boot_verified", "true")) {
				return -EFAULT;
			}
		}
		return 0;
	}

	if (!strcmp(nvram_get("sys_boot_verified"), "true")) {
		if (nvram_get("sys_boot_start")) {
			/* mode verified */
			printf("%s: swap successfull\n", __func__);
			if (nvram_set("sys_boot_part", nvram_get("sys_boot_swap"))) {
				return -EFAULT;
			}
			if (nvram_set("sys_boot_start", NULL)) {
				return -EFAULT;
			}

		}
		else {
			/* mode init swap */
			printf("%s: swap initiated\n", __func__);
			if (nvram_set("sys_boot_start", nvram_get("sys_bootcount"))) {
				return -EFAULT;
			}
			if (nvram_set("sys_boot_verified", "false")) {
				return -EFAULT;
			}
			nvram_set_env("sys_boot_swap", "mmcpart");
		}
		return 0;
	}

	/* mode in progress */
	const ulong bootcount = nvram_get_ulong("sys_bootcount", 10, 0);
	const ulong start = nvram_get_ulong("sys_boot_start", 10, 0);
	const ulong diff = (start < bootcount) ? bootcount - start : CONFIG_DR_NVRAM_BOOT_SWAP_RETRIES;
	printf("%s: swap in progress: bootcount: %lu\n", __func__, diff);
	if (diff >= (ulong) CONFIG_DR_NVRAM_BOOT_SWAP_RETRIES) {
		/* mode rollback */
		printf("%s: max retries reached (%lu >= %lu): rollback\n", __func__, diff, (ulong) CONFIG_DR_NVRAM_BOOT_SWAP_RETRIES);
		if (nvram_set("sys_boot_swap", nvram_get("sys_boot_part"))) {
			return -EFAULT;
		}
		if (nvram_set("sys_boot_start", NULL)) {
			return -EFAULT;
		}
		if (nvram_set("sys_boot_verified", "true")) {
			return -EFAULT;
		}
	}

	nvram_set_env("sys_boot_swap", "mmcpart");
	return 0;
}
#endif

int board_late_init(void)
{
	int r = 0;
#if defined(CONFIG_DR_NVRAM_BOOTCOUNT)
	if ((r = increment_bootcounter())) {
		printf("Failed incrementing bootcounter [%d]: %s\n", -r, errno_str(-r));
	}
#endif
#if defined (CONFIG_DR_NVRAM)
	r = nvram_init_env();
	if (r) {
		printf("Failed setting nvram env [%d]: %s\n", -r, errno_str(-r));
	}
#if defined (CONFIG_DR_NVRAM_BOOT_SWAP)
	r = nvram_boot_swap();
	if (r) {
		printf("Failed boot swap procedure [%d]: %s\n", -r, errno_str(-r));
	}
#endif
#endif
#if defined(CONFIG_DR_NVRAM_BOOTSPLASH)
	printf("Enabling bootsplash...\n");
	if ((r = bootsplash_load())) {
		printf("Failed loading bootsplash [%d]: %s\n", -r, errno_str(-r));
	}
#endif
	select_fdt();
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
#endif
#if defined(CONFIG_DR_NVRAM)
	r = nvram_commit();
	if (r) {
		printf("Failed commiting nvram [%d]: %s\n", -r, errno_str(-r));
	}
#endif

	return 0;
}
