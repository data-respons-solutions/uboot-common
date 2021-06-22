#include <common.h>
#include <command.h>
#include <env.h>
#include <part.h>
#include <asm/mach-imx/hab.h>
#include <linux/libfdt.h>
#include "../include/bootsplash.h"
#include "../include/nvram.h"

static const char* syslabel_default = "rootfs1";
static const char* syslabel = "syslabel";
static const char* syspart = "syspart";
static const char* sysdev = "sysdev";
static const char* sysiface = "sysiface";
static const char* sysuuid = "sysuuid";
static const char* envfitconf = "fit_conf";

#if defined(CONFIG_DR_NVRAM)

static const char* sys_fit_conf = "SYS_FIT_CONF";

static int fit_conf(void)
{
	const char* conf = nvram_get(sys_fit_conf);
	if (conf) {
		env_set(envfitconf, conf);
		printf("FIT conf: %s\n", conf);
	}
	return 0;
}
#endif

#if defined(CONFIG_DR_NVRAM_ROOT_SWAP)

static const char* sys_boot_part = "SYS_BOOT_PART";
static const char* sys_boot_swap = "SYS_BOOT_SWAP";
static const char* sys_boot_attempts = "SYS_BOOT_ATTEMPTS";

enum swap_state {
	SWAP_NORMAL,
	SWAP_INIT,
	SWAP_ONGOING,
	SWAP_FAILED,
	SWAP_ROLLBACK,
	SWAP_INVAL,
};

static enum swap_state find_state(ulong* attempts)
{
	if (!nvram_get(sys_boot_part) || !nvram_get(sys_boot_swap))
		return SWAP_INVAL;

	const int part_swap_equal = !strcmp(nvram_get(sys_boot_part), nvram_get(sys_boot_swap));
	if (part_swap_equal && !nvram_get(sys_boot_attempts))
		return SWAP_NORMAL;

	if (part_swap_equal && nvram_get(sys_boot_attempts))
		return SWAP_ROLLBACK;

	if (!nvram_get(sys_boot_attempts))
		return SWAP_INIT;

	*attempts = nvram_get_ulong(sys_boot_attempts, 10, ULONG_MAX);
	if (*attempts == ULONG_MAX)
		return SWAP_INVAL;

	if (*attempts < 3)
		return SWAP_ONGOING;

	return SWAP_FAILED;
}

static int nvram_root_swap(void)
{
	char* label = nvram_get(sys_boot_part);
	ulong attempts = ULONG_MAX;

	switch(find_state(&attempts)) {
	case SWAP_NORMAL:
		printf("Root swap: normal boot\n");
		break;
	case SWAP_INIT:
		printf("Root swap: initiated\n");
		nvram_set_ulong(sys_boot_attempts, 1);
		label = nvram_get(sys_boot_swap);
		break;
	case SWAP_ONGOING:
		printf("Root swap: ongoing\n");
		nvram_set_ulong(sys_boot_attempts, ++attempts);
		label = nvram_get(sys_boot_swap);
		break;
	case SWAP_FAILED:
		printf("Root swap: failed, rollback now\n");
		nvram_set(sys_boot_swap, nvram_get(sys_boot_part));
		break;
	case SWAP_ROLLBACK:
		printf("Root swap: rollback has occured\n");
		break;
	case SWAP_INVAL:
		printf("Root swap: invalid state -- reset\n");
		if (nvram_set(sys_boot_part, syslabel_default))
			return -ENOMEM;
		if (nvram_set(sys_boot_swap, syslabel_default))
			return -ENOMEM;
		if (nvram_set(sys_boot_attempts, NULL))
			return -ENOMEM;
		label = nvram_get(sys_boot_part);
		break;
	}

	if (env_set(syslabel, label)) {
		return -ENOMEM;
	}
	return 0;
}
#endif

static int part_from_label(void)
{
	if (!env_get(syslabel) && env_set(syslabel, syslabel_default))
		return -ENOMEM;

	if (env_set(sysiface, SYS_BOOT_IFACE))
		return -ENOMEM;
	if (env_set_ulong(sysdev, SYS_BOOT_DEV))
		return -ENOMEM;

	printf("Finding syspart with syslabel \"%s\" on device %s %s\n", env_get(syslabel), env_get(sysiface), env_get(sysdev));

	struct blk_desc* dev = blk_get_dev(env_get(sysiface), env_get_ulong(sysdev, 10, ULONG_MAX));
	if (!dev) {
		printf("Failed getting system boot device\n");
		return -EFAULT;
	}

	disk_partition_t part_info;
	int part = part_get_info_by_name(dev, env_get(syslabel), &part_info);
	if (part < 1) {
		printf("syslabel not found\n");
		return -EINVAL;
	}
	if(env_set_ulong(syspart, part))
		return -ENOMEM;
	if(env_set(sysuuid, part_info.uuid))
		return -ENOMEM;

	printf("Found syspart %s with sysuuid: %s\n", env_get(syspart), env_get(sysuuid));

	return 0;
}

int board_late_init(void)
{
	int r = 0;
#if defined (CONFIG_DR_NVRAM)
	r = nvram_init();
	if (r)
		printf("Failed nvram_init [%d]: %s\n", r, errno_str(r));
	else {
		r = fit_conf();
		if (r)
			printf("Failed setting fit_conf: %d\n", r);

#if defined (CONFIG_DR_NVRAM_ROOT_SWAP)
		r = nvram_root_swap();
		if (r)
			printf("Failed root swap procedure [%d]: %s\n", r, errno_str(r));
#endif
		r = nvram_commit();
		if (r)
			printf("Failed commiting nvram [%d]: %s\n", r, errno_str(r));
	}
#endif
#if defined(CONFIG_DR_NVRAM_BOOTSPLASH)
	printf("Enabling bootsplash...\n");
	if ((r = bootsplash_load()))
		printf("Failed loading bootsplash [%d]: %s\n", r, errno_str(r));
#endif

	r = part_from_label();
	if (r)
		printf("Failed getting system partition [%d]: %s\n", r, errno_str(r));

	return 0;
}
