#include <common.h>
#include <stdlib.h>
#include <errno.h>
#include <spi_flash.h>
#include <inttypes.h>
#include "../include/nvram.h"
#include "../libnvram/libnvram.h"

static struct nvram* nvram = NULL;

struct nvram {
	struct udevice *flash;
	struct nvram_transaction trans;
	struct nvram_list *list;
	int list_updated;
};

static int probe_flash(struct udevice** flash)
{
	int r = spi_flash_probe_bus_cs(CONFIG_DR_NVRAM_BUS, CONFIG_DR_NVRAM_CS,
								CONFIG_DR_NVRAM_SPEED, CONFIG_DR_NVRAM_MODE,
								flash);
	if (r) {
		printf("%s: failed probing nvram [%d]: %s\n", __func__, r, errno_str(r));
		return r;
	}

	return 0;
}

static int read_section(struct udevice* flash, uint32_t offset, uint8_t** data, uint32_t len)
{
	uint8_t *buf = malloc(len);
	if (!buf) {
		printf("%s: failed allocating memory: %u bytes\n", __func__, len);
		return -ENOMEM;
	}
	int r = spi_flash_read_dm(flash, offset, len, buf);
	if (r) {
		free(buf);
		printf("%s: failed reading nvram [%d]: %s\n", __func__, r, errno_str(r));
		return r;
	}
	*data = buf;

	return 0;
}

static const char* nvram_active_str(enum nvram_active active)
{
	switch (active) {
	case NVRAM_ACTIVE_A:
		return "A";
	case NVRAM_ACTIVE_B:
		return "B";
	default:
		return "NONE";
	}
}

int nvram_init(void)
{
	if (nvram) {
		return -EALREADY;
	}

	uint8_t *buf_a = NULL;
	uint8_t *buf_b = NULL;
	nvram = (struct nvram*) malloc(sizeof(struct nvram));
	if (!nvram) {
		return ENOMEM;
	}
	memset(nvram, 0, sizeof(struct nvram));

	int r = probe_flash(&nvram->flash);
	if (r) {
		return r;
	}

	r = read_section(nvram->flash, CONFIG_DR_NVRAM_SECTION_A_START, &buf_a, CONFIG_DR_NVRAM_SECTION_A_SIZE);
	if (r) {
		goto exit;
	}
	r = read_section(nvram->flash, CONFIG_DR_NVRAM_SECTION_B_START, &buf_b, CONFIG_DR_NVRAM_SECTION_B_SIZE);
	if (r) {
		goto exit;
	}

	nvram_init_transaction(&nvram->trans, buf_a, CONFIG_DR_NVRAM_SECTION_A_SIZE, buf_b, CONFIG_DR_NVRAM_SECTION_B_SIZE);
	printf("nvram: active: %s\n", nvram_active_str(nvram->trans.active));
	r = 0;
	if ((nvram->trans.active & NVRAM_ACTIVE_A) == NVRAM_ACTIVE_A) {
		r = nvram_deserialize(&nvram->list, buf_a + nvram_header_len(), CONFIG_DR_NVRAM_SECTION_A_SIZE - nvram_header_len(), &nvram->trans.section_a.hdr);
	}
	else
	if ((nvram->trans.active & NVRAM_ACTIVE_B) == NVRAM_ACTIVE_B) {
		r = nvram_deserialize(&nvram->list, buf_b + nvram_header_len(), CONFIG_DR_NVRAM_SECTION_B_SIZE - nvram_header_len(), &nvram->trans.section_b.hdr);
	}

	if (r) {
		printf("failed deserializing data [%d]: %s\n", -r, errno_str(r));
		goto exit;
	}

	r = 0;
exit:
	if (buf_a) {
		free(buf_a);
	}
	if (buf_b) {
		free(buf_b);
	}
	if (r) {
		if (nvram) {
			free(nvram);
			nvram = NULL;
		}
	}
	return r;
}

static int write_section(struct udevice* flash, uint32_t offset, const uint8_t* data, uint32_t len)
{
	int r = spi_flash_erase_dm(flash, offset, len);
	if (r) {
		printf("%s: failed erasing nvram [%d]: %s\n", __func__, r, errno_str(r));
		return r;
	}

	r = spi_flash_write_dm(flash, offset, len, data);
	if (r) {
		printf("%s: failed writing nvram [%d]: %s\n", __func__, r, errno_str(r));
		return r;
	}

	return 0;
}

int nvram_commit(void)
{
	if (!nvram) {
		return -ENXIO;
	}
	if (!nvram->list_updated) {
		return 0;
	}

	uint8_t *buf = NULL;
	int r = 0;
	uint32_t size = nvram_serialize_size(nvram->list);
	if (size > CONFIG_DR_NVRAM_SECTION_A_SIZE || size > CONFIG_DR_NVRAM_SECTION_B_SIZE) {
		printf("nvram: serialied size does not fit in flash");
		return -EFBIG;
	}

	buf = (uint8_t*) malloc(size);
	if (!buf) {
		printf("failed allocating %" PRIu32 " byte write buffer\n", size);
		return -ENOMEM;
	}

	struct nvram_header hdr;
	enum nvram_operation op = nvram_next_transaction(&nvram->trans, &hdr);
	uint32_t bytes = nvram_serialize(nvram->list, buf, size, &hdr);
	if (!bytes) {
		printf("nvram: failed serializing list\n");
		r = -ENOMEM;
		goto exit;
	}

	const int is_write_a = (op & NVRAM_OPERATION_WRITE_A) == NVRAM_OPERATION_WRITE_A;
	const int is_counter_reset = (op & NVRAM_OPERATION_COUNTER_RESET) == NVRAM_OPERATION_COUNTER_RESET;
	// first write
	if (is_write_a) {
		r = write_section(nvram->flash, CONFIG_DR_NVRAM_SECTION_A_START, buf, CONFIG_DR_NVRAM_SECTION_A_SIZE);
	}
	else {
		r = write_section(nvram->flash, CONFIG_DR_NVRAM_SECTION_B_START, buf, CONFIG_DR_NVRAM_SECTION_B_SIZE);
	}
	if (!r && is_counter_reset) {
		// second write, if requested
		if (is_write_a) {
			r = write_section(nvram->flash, CONFIG_DR_NVRAM_SECTION_B_START, buf, CONFIG_DR_NVRAM_SECTION_B_SIZE);
		}
		else {
			r = write_section(nvram->flash, CONFIG_DR_NVRAM_SECTION_A_START, buf, CONFIG_DR_NVRAM_SECTION_A_SIZE);
		}
	}
	if (r) {
		goto exit;
	}

	nvram_update_transaction(&nvram->trans, op, &hdr);
	printf("nvram: active: %s\n", nvram_active_str(nvram->trans.active));
	nvram->list_updated = 0;

	r = 0;
exit:
	if (buf) {
		free(buf);
	}
	return r;
}

char *nvram_get(const char* varname)
{
	if (!varname || !nvram) {
		return NULL;
	}
	struct nvram_entry *entry = nvram_list_get(nvram->list, (uint8_t*) varname, strlen(varname) + 1);
	if (entry && entry->value[entry->value_len - 1] == '\0') {
		return (char*) entry->value;
	}
	// not entry->value not string
	return NULL;
}

ulong nvram_get_ulong(const char* varname, int base, ulong default_value)
{
	const char *str = nvram_get(varname);
	return str ? simple_strtoul(str, NULL, base) : default_value;
}

static int starts_with(const char* str, const char* prefix)
{
	size_t str_len = strlen(str);
	size_t prefix_len = strlen(prefix);
	if (str_len > prefix_len) {
		if(!strncmp(str, prefix, prefix_len)) {
			return 1;
		}
	}
	return 0;
}

int nvram_set(const char* varname, const char* value)
{
	if (!varname || !nvram) {
		return 1;
	}

	if (!starts_with(varname, CONFIG_DR_NVRAM_VARIABLE_PREFIX)) {
		printf("%s: varname not prefixed with %s\n", __func__, CONFIG_DR_NVRAM_VARIABLE_PREFIX);
		return 1;
	}

	if (!value || !strlen(value)) {
		if (nvram_list_remove(&nvram->list, (uint8_t*) varname, strlen(varname) + 1)) {
			nvram->list_updated = 1;
			return 0;
		}
	}
	else {
		const char *_val = nvram_get(varname);
		if (_val && !strcmp(_val, value)) {
			// already equal
			return 0;
		}
		struct nvram_entry entry;
		entry.key = (uint8_t*) varname;
		entry.key_len = strlen(varname) + 1;
		entry.value = (uint8_t*) value;
		entry.value_len = strlen(value) + 1;
		if (!nvram_list_set(&nvram->list, &entry)) {
			nvram->list_updated = 1;
			return 0;
		}
	}

	return 1;
}

int nvram_set_ulong(const char* varname, ulong value)
{
	char *str = simple_itoa(value);
	return nvram_set(varname, str);
}

int nvram_set_env(const char* varname, const char* envname)
{
	const char* var = nvram_get(varname);
	return var ? env_set(envname, var) : 1;
}

struct nvram_list* const nvram_get_list(void)
{
	if (!nvram) {
		return NULL;
	}
	return nvram->list;
}
