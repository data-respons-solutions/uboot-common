#include <common.h>
#include <stdlib.h>
#include <errno.h>
#include <spi_flash.h>
#include "../include/nvram.h"
#include "../libnvram/libnvram.h"

static struct nvram_data* nvram_priv = NULL;
static struct udevice* flash = NULL;

enum flash_section {
	SECTION_UNKNOWN = 0,
	SECTION_A,
	SECTION_B,
};

struct nvram_data {
	enum flash_section active;
	uint32_t counter;
	struct nvram_list list;
};

static int probe_flash(void)
{
	if (!flash) {
		int r = spi_flash_probe_bus_cs(CONFIG_DR_NVRAM_BUS, CONFIG_DR_NVRAM_CS,
									CONFIG_DR_NVRAM_SPEED, CONFIG_DR_NVRAM_MODE,
									&flash);
		if (r) {
			printf("%s: failed probing nvram [%d]: %s\n", __func__, -r, errno_str(-r));
			return r;
		}
	}

	return 0;
}

static int read_section(enum flash_section section, uint8_t* buf)
{
	int r = 0;

	r = probe_flash();
	if (r) {
		return r;
	}

	const uint32_t flash_offset = section == SECTION_A ? CONFIG_DR_NVRAM_SECTION_A_START : CONFIG_DR_NVRAM_SECTION_B_START;
	const uint32_t flash_size = section == SECTION_A ? CONFIG_DR_NVRAM_SECTION_A_SIZE : CONFIG_DR_NVRAM_SECTION_B_SIZE;
	r = spi_flash_read_dm(flash, flash_offset, flash_size, buf);
	if (r) {
		printf("%s: failed reading nvram [%d]: %s\n", __func__, -r, errno_str(-r));
		return r;
	}

	return 0;
}

static int write_section(enum flash_section section, const uint8_t* buf, uint32_t buf_len)
{
	int r = 0;

	r = probe_flash();
	if (r) {
		return r;
	}

	const uint32_t flash_offset = section == SECTION_A ? CONFIG_DR_NVRAM_SECTION_A_START : CONFIG_DR_NVRAM_SECTION_B_START;
	const uint32_t flash_size = section == SECTION_A ? CONFIG_DR_NVRAM_SECTION_A_SIZE : CONFIG_DR_NVRAM_SECTION_B_SIZE;
	if (buf_len > flash_size) {
		printf("%s: buffer [%ub] larger than flash [%ub]\n", __func__, buf_len, flash_size);
		return -EINVAL;
	}

	r = spi_flash_erase_dm(flash, flash_offset, flash_size);
	if (r) {
		printf("%s: failed erasing nvram [%d]: %s\n", __func__, -r, errno_str(-r));
		return r;
	}

	r = spi_flash_write_dm(flash, flash_offset, buf_len, buf);
	if (r) {
		printf("%s: failed writing nvram [%d]: %s\n", __func__, -r, errno_str(-r));
		return r;
	}

	return 0;
}

static int nvram_load(void)
{
	if (nvram_priv) {
		printf("%s: nvram already loaded\n", __func__);
		return -EINVAL;
	}

	nvram_priv = malloc(sizeof(struct nvram_data));
	if (!nvram_priv) {
		printf("%s: failed allocating memory for priv data\n", __func__);
		return -ENOMEM;
	}

	nvram_priv->counter = 0;
	nvram_priv->active = SECTION_UNKNOWN;
	nvram_priv->list.entry = NULL;

	int r = 0;
	uint8_t* _section_a = NULL;
	uint8_t* _section_b = NULL;

	_section_a = malloc(CONFIG_DR_NVRAM_SECTION_A_SIZE);
	if (!_section_a) {
		printf("%s: failed allocating memory (A): %d bytes\n", __func__, CONFIG_DR_NVRAM_SECTION_A_SIZE);
		r = -ENOMEM;
		goto exit;
	}
	_section_b = malloc(CONFIG_DR_NVRAM_SECTION_B_SIZE);
	if (!_section_b) {
		printf("%s: failed allocating memory (B): %d bytes\n", __func__, CONFIG_DR_NVRAM_SECTION_B_SIZE);
		r = -ENOMEM;
		goto exit;
	}

	r = read_section(SECTION_A, _section_a);
	if (r) {
		goto exit;
	}
	r = read_section(SECTION_B, _section_b);
	if (r) {
		goto exit;
	}
	uint32_t counter_a = 0;
	uint32_t data_len_a = 0;
	int is_valid_a = is_valid_nvram_section(_section_a, CONFIG_DR_NVRAM_SECTION_A_SIZE, &data_len_a, &counter_a);
	uint32_t counter_b = 0;
	uint32_t data_len_b = 0;
	int is_valid_b = is_valid_nvram_section(_section_b, CONFIG_DR_NVRAM_SECTION_B_SIZE, &data_len_b, &counter_b);

	if (is_valid_a && (counter_a > counter_b)) {
		nvram_priv->active = SECTION_A;
		nvram_priv->counter = counter_a;
	}
	else
	if (is_valid_b) {
		nvram_priv->active = SECTION_B;
		nvram_priv->counter = counter_b;
	}
	else {
		printf("%s: no active section found\n", __func__);
		r = -EINVAL;
		goto exit;
	}

	r = nvram_section_deserialize(&nvram_priv->list,
									nvram_priv->active == SECTION_A ? _section_a : _section_b,
									nvram_priv->active == SECTION_A ? data_len_a : data_len_b);
	if (r) {
		printf("%s: failed deserializing data [%d]: %s\n", __func__, -r, errno_str(-r));
		goto exit;
	}

	r = 0;

exit:
	if (_section_a) {
		free(_section_a);
	}
	if (_section_b) {
		free(_section_b);
	}
	return r;
}

static int nvram_store(void)
{
	if (!nvram_priv) {
		return -EINVAL;
	}

	uint8_t* buf = NULL;
	int r = 0;

	uint32_t size = 0;
	r = nvram_section_serialize_size(&nvram_priv->list, &size);
	if (r) {
		printf("%s: failed getting serialized buffer size [%d]: %s\n", __func__, -r, errno_str(-r));
		goto exit;
	}

	buf = malloc(size);
	if (!buf) {
		printf("%s: failed allocating memory: %u bytes\n", __func__, size);
		r = -ENOMEM;
		goto exit;
	}

	r = nvram_section_serialize(&nvram_priv->list, nvram_priv->counter + 1, buf, size);
	if (r) {
		printf("%s: failed serializing data [%d]: %s\n", __func__, -r, errno_str(-r));
		goto exit;
	}

	switch(nvram_priv->active) {
	case SECTION_A:
		r = write_section(SECTION_B, buf, size);
		if (r) {
			goto exit;
		}
		break;
	case SECTION_B:
		r = write_section(SECTION_A, buf, size);
		if (r) {
			goto exit;
		}
		break;
	default:
		printf("%s: no active setion defined\n", __func__);
		r = -EBADF;
		goto exit;
	}

	r = 0;

exit:
	if (buf) {
		free(buf);
	}
	return r;
}

char *nvram_get(const char* varname)
{
	if (!varname) {
		return NULL;
	}

	if (!nvram_priv) {
		if (nvram_load()) {
			return NULL;
		}
	}
	printf("getting: %s\n", varname);
	return nvram_list_get(&nvram_priv->list, varname);
}

int nvram_set(const char* varname, const char* value)
{
	if (!varname) {
		return 1;
	}

	if (!nvram_priv) {
		if (nvram_load()) {
			return 1;
		}
	}

	if (!value || !strlen(value)) {
		if (!nvram_list_remove(&nvram_priv->list, varname)) {
			return 0;
		}
	}
	else {
		if (!nvram_list_set(&nvram_priv->list, varname, value)) {
			return 0;
		}
	}

	return 1;
}

int nvram_commit(void)
{
	if (!nvram_priv) {
		return 0;
	}

	int r = nvram_store();
	if (r) {
		return r;
	}

	destroy_nvram_list(&nvram_priv->list);
	free(nvram_priv);
	nvram_priv = NULL;

	return 0;
}

struct nvram_list* const nvram_get_list(void)
{
	if (!nvram_priv) {
		if(nvram_load()) {
			return NULL;
		}
	}

	return &nvram_priv->list;
}
