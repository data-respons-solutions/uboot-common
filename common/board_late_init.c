#include <common.h>
#include <command.h>
#include <asm/mach-imx/hab.h>
#include <linux/libfdt.h>
#include "../include/bootsplash.h"
#include "../include/nvram.h"

static const char* VAR_BOOT_PART = CONFIG_DR_NVRAM_VARIABLE_PREFIX "" CONFIG_DR_NVRAM_VARIABLE_BOOT_PART;
static const char* VAR_BOOT_SWAP = CONFIG_DR_NVRAM_VARIABLE_PREFIX "" CONFIG_DR_NVRAM_VARIABLE_BOOT_SWAP;
static const char* VAR_BOOT_VERIFIED = CONFIG_DR_NVRAM_VARIABLE_PREFIX "" CONFIG_DR_NVRAM_VARIABLE_BOOT_VERIFIED;
static const char* VAR_BOOT_RETRIES = CONFIG_DR_NVRAM_VARIABLE_PREFIX "" CONFIG_DR_NVRAM_VARIABLE_BOOT_RETRIES;

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

#if defined(CONFIG_DR_NVRAM)
static int nvram_init_env(void)
{
	const char* part = nvram_get(VAR_BOOT_PART);
	if (!part) {
		printf("%s: %s: reset to default: \"%s\"\n", __func__, VAR_BOOT_PART, DEFAULT_MMC_PART);
		if (nvram_set(VAR_BOOT_PART, DEFAULT_MMC_PART)) {
			return -EFAULT;
		}
	}

	if (nvram_set_env(VAR_BOOT_PART, "mmcpart")) {
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
 * mode        | _verified |  _swap  |     _retries     |
 * normal      |      x    |  _part  |       x          |
 * init swap   |    true   |  !_part |      NULL        |
 * swapping    |    false  |  !_part |     < MAX        |
 * rollback    |    false  |  !_part |    >= MAX        |
 * verified    |    true   |  !_part |     !NULL        |
 *
 */

static int nvram_boot_swap(void)
{
	const char* verified = nvram_get(VAR_BOOT_VERIFIED);
	if (!verified) {
		printf("%s: %s: reset to default: \"true\"\n", __func__, VAR_BOOT_VERIFIED);
		if (nvram_set(VAR_BOOT_VERIFIED, "true")) {
			return -EFAULT;
		}
	}
	const char* swap = nvram_get(VAR_BOOT_SWAP);
	if (!swap) {
		printf("%s: %s: reset to default: \"%s\"\n", __func__, VAR_BOOT_SWAP, nvram_get(VAR_BOOT_PART));
		if (nvram_set(VAR_BOOT_SWAP, nvram_get(VAR_BOOT_PART))) {
			return -EFAULT;
		}
	}

	if (!strcmp(nvram_get(VAR_BOOT_PART), nvram_get(VAR_BOOT_SWAP))) {
		/* mode normal */
		printf("%s: verified state: p%s\n", __func__, nvram_get(VAR_BOOT_PART));
		if (strcmp(nvram_get(VAR_BOOT_VERIFIED), "true")) {
			printf("%s: setting %s to true\n", __func__, VAR_BOOT_VERIFIED);
			if (nvram_set(VAR_BOOT_VERIFIED, "true")) {
				return -EFAULT;
			}
		}
		return 0;
	}

	if (!strcmp(nvram_get(VAR_BOOT_VERIFIED), "true")) {
		if (nvram_get(VAR_BOOT_RETRIES)) {
			/* mode verified */
			printf("%s: swap from p%s to p%s successful\n", __func__, nvram_get(VAR_BOOT_PART), nvram_get(VAR_BOOT_SWAP));
			if (nvram_set(VAR_BOOT_PART, nvram_get(VAR_BOOT_SWAP))) {
				return -EFAULT;
			}
			if (nvram_set(VAR_BOOT_RETRIES, NULL)) {
				return -EFAULT;
			}

		}
		else {
			/* mode init swap */
			printf("%s: swap initiated: p%s -> p%s\n", __func__, nvram_get(VAR_BOOT_PART), nvram_get(VAR_BOOT_SWAP));
			if (nvram_set_ulong(VAR_BOOT_RETRIES, 0)) {
				return -EFAULT;
			}
			if (nvram_set(VAR_BOOT_VERIFIED, "false")) {
				return -EFAULT;
			}
			nvram_set_env(VAR_BOOT_SWAP, "mmcpart");
		}
		return 0;
	}

	/* mode in progress */
	const ulong retries = nvram_get_ulong(VAR_BOOT_RETRIES, 10, 0) + 1;
	nvram_set_ulong(VAR_BOOT_RETRIES, retries);
	printf("%s: swap from p%s to p%s in progress: retries: %lu\n", __func__, nvram_get(VAR_BOOT_PART), nvram_get(VAR_BOOT_SWAP), retries);
	if (retries >= (ulong) CONFIG_DR_NVRAM_BOOT_SWAP_RETRIES) {
		/* mode rollback */
		printf("%s: max retries reached (%lu >= %lu): rollback\n", __func__, retries, (ulong) CONFIG_DR_NVRAM_BOOT_SWAP_RETRIES);
		if (nvram_set(VAR_BOOT_SWAP, nvram_get(VAR_BOOT_PART))) {
			return -EFAULT;
		}
		if (nvram_set(VAR_BOOT_RETRIES, NULL)) {
			return -EFAULT;
		}
		if (nvram_set(VAR_BOOT_VERIFIED, "true")) {
			return -EFAULT;
		}
	}

	nvram_set_env(VAR_BOOT_SWAP, "mmcpart");
	return 0;
}
#endif

int board_late_init(void)
{
	int r = 0;
#if defined (CONFIG_DR_NVRAM)
	r = nvram_init();
	if (r) {
		printf("Failed nvram_init [%d]: %s\n", r, errno_str(r));
	}
	else {
		r = nvram_init_env();
		if (r) {
			printf("Failed setting nvram env [%d]: %s\n", r, errno_str(r));
		}
#if defined (CONFIG_DR_NVRAM_BOOT_SWAP)
		r = nvram_boot_swap();
		if (r) {
			printf("Failed boot swap procedure [%d]: %s\n", r, errno_str(r));
		}
#endif
		r = nvram_commit();
		if (r) {
			printf("Failed commiting nvram [%d]: %s\n", r, errno_str(r));
		}
	}
#endif
#if defined(CONFIG_DR_NVRAM_BOOTSPLASH)
	printf("Enabling bootsplash...\n");
	if ((r = bootsplash_load())) {
		printf("Failed loading bootsplash [%d]: %s\n", r, errno_str(r));
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

	return 0;
}
