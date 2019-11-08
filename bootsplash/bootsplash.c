#include <common.h>
#include <command.h>
#include <stdlib.h>
#include <errno.h>
#include <spi_flash.h>
#include <lcd.h>
#include "bootsplash.h"

static struct udevice* flash = NULL;

static int probe_flash(void)
{
	if (!flash) {
		int r = spi_flash_probe_bus_cs(CONFIG_DR_NVRAM_BUS, CONFIG_DR_NVRAM_CS,
									CONFIG_DR_NVRAM_SPEED, CONFIG_DR_NVRAM_MODE,
									&flash);
		if (r) {
			printf("%s: failed probing bootsplash [%d]: %s\n", __func__, -r, errno_str(-r));
			return r;
		}
	}

	return 0;
}

int bootsplash_load(void)
{
	int r = 0;

	r = probe_flash();
	if (r) {
		return r;
	}

	uint8_t* buf = malloc(CONFIG_DR_BOOTSPLASH_SIZE);
	if (!buf) {
		printf("%s: failed allocating memory: %d bytes\n", __func__, CONFIG_DR_BOOTSPLASH_SIZE);
		r = -ENOMEM;
		goto exit;
	}

	r = spi_flash_read_dm(flash, CONFIG_DR_BOOTSPLASH_START, CONFIG_DR_BOOTSPLASH_SIZE, buf);
	if (r) {
		printf("%s: failed reading bootsplash [%d]: %s\n", __func__, -r, errno_str(-r));
		return r;
	}

	if (bmp_display((ulong) buf, 0, 0)) {
		r = -EFAULT;
		goto exit;
	}

	r =  0;

exit:
	if (buf) {
		free(buf);
	}
	return r;
}
