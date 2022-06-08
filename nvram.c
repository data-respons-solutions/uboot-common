#include <common.h>
#include <stdlib.h>
#include <errno.h>
#include <spi_flash.h>
#include <inttypes.h>
#include <linux/ctype.h>
#include "nvram.h"
#include "libnvram/libnvram.h"

struct nvram {
	struct udevice *flash;
	struct libnvram_transaction trans;
	struct libnvram_list *list;
	int list_updated;
};

static struct nvram* nvram = NULL;

static int probe_flash(struct udevice** flash)
{
	int r = spi_flash_probe_bus_cs(CONFIG_DR_NVRAM_BUS, CONFIG_DR_NVRAM_CS,
								CONFIG_DR_NVRAM_SPEED, CONFIG_DR_NVRAM_MODE,
								flash);
	if (r) {
		pr_err("nvram: failed probing spi %d:%d: %d\n", CONFIG_DR_NVRAM_BUS, CONFIG_DR_NVRAM_CS, r);
		return r;
	}
	pr_debug("nvram: probed spi %d:%d: %s\n", CONFIG_DR_NVRAM_BUS, CONFIG_DR_NVRAM_CS, (*flash)->name);

	return 0;
}

static int read_section(struct udevice* flash, uint32_t offset, uint8_t** data, uint32_t len)
{
	uint8_t *buf = malloc(len);
	if (!buf)
		return -ENOMEM;

	int r = spi_flash_read_dm(flash, offset, len, buf);
	if (r) {
		free(buf);
		pr_err("nvram: failed reading %s: %d\n", flash->name, r);
		return r;
	}
	*data = buf;

	return 0;
}

static const char* active_str(enum libnvram_active active)
{
	switch (active) {
	case LIBNVRAM_ACTIVE_A:
		return "A";
	case LIBNVRAM_ACTIVE_B:
		return "B";
	default:
		return "NONE";
	}
}

int nvram_init(void)
{
	if (nvram)
		return 0;

	uint8_t *buf_a = NULL;
	uint8_t *buf_b = NULL;
	nvram = (struct nvram*) malloc(sizeof(struct nvram));
	if (!nvram)
		return -ENOMEM;

	memset(nvram, 0, sizeof(struct nvram));

	int r = probe_flash(&nvram->flash);
	if (r)
		return r;
	r = read_section(nvram->flash, CONFIG_DR_NVRAM_SECTION_A_START, &buf_a, CONFIG_DR_NVRAM_SECTION_A_SIZE);
	if (r)
		goto exit;
	r = read_section(nvram->flash, CONFIG_DR_NVRAM_SECTION_B_START, &buf_b, CONFIG_DR_NVRAM_SECTION_B_SIZE);
	if (r)
		goto exit;

	libnvram_init_transaction(&nvram->trans, buf_a, CONFIG_DR_NVRAM_SECTION_A_SIZE, buf_b, CONFIG_DR_NVRAM_SECTION_B_SIZE);
	pr_info("nvram: active: %s\n", active_str(nvram->trans.active));
	if ((nvram->trans.active & LIBNVRAM_ACTIVE_A) == LIBNVRAM_ACTIVE_A) {
		r = libnvram_deserialize(&nvram->list, buf_a + libnvram_header_len(), CONFIG_DR_NVRAM_SECTION_A_SIZE - libnvram_header_len(), &nvram->trans.section_a.hdr);
	}
	else
	if ((nvram->trans.active & LIBNVRAM_ACTIVE_B) == LIBNVRAM_ACTIVE_B) {
		r = libnvram_deserialize(&nvram->list, buf_b + libnvram_header_len(), CONFIG_DR_NVRAM_SECTION_B_SIZE - libnvram_header_len(), &nvram->trans.section_b.hdr);
	}

	if (r) {
		pr_err("nvram: deserialize libnvram_error: %d\n", r);
		r = -ENOMEM;
		goto exit;
	}

	r = 0;
exit:
	if (buf_a)
		free(buf_a);
	if (buf_b)
		free(buf_b);
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
		pr_err("nvram: failed erasing %s: %d\n", flash->name, r);
		return r;
	}

	r = spi_flash_write_dm(flash, offset, len, data);
	if (r) {
		pr_err("nvram: failed writing %s: %d\n", flash->name, r);
		return r;
	}

	return 0;
}

int nvram_commit(void)
{
	if (!nvram)
		return -ENXIO;
	if (!nvram->list_updated)
		return 0;

	uint8_t *buf = NULL;
	int r = 0;
	uint32_t size = libnvram_serialize_size(nvram->list, LIBNVRAM_TYPE_LIST);
	if (size > CONFIG_DR_NVRAM_SECTION_A_SIZE || size > CONFIG_DR_NVRAM_SECTION_B_SIZE) {
		pr_err("nvram: serialied size does not fit in flash");
		return -EFBIG;
	}

	buf = (uint8_t*) malloc(size);
	if (!buf) {
		pr_err("nvram: failed allocating %" PRIu32 " byte write buffer\n", size);
		return -ENOMEM;
	}

	struct libnvram_header hdr;
	hdr.type = LIBNVRAM_TYPE_LIST;
	enum libnvram_operation op = libnvram_next_transaction(&nvram->trans, &hdr);
	if (!libnvram_serialize(nvram->list, buf, size, &hdr)) {
		pr_err("nvram: failed serialize\n");
		r = -ENOMEM;
		goto exit;
	}

	const int is_write_a = (op & LIBNVRAM_OPERATION_WRITE_A) == LIBNVRAM_OPERATION_WRITE_A;
	const int is_counter_reset = (op & LIBNVRAM_OPERATION_COUNTER_RESET) == LIBNVRAM_OPERATION_COUNTER_RESET;
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
	if (r)
		goto exit;

	libnvram_update_transaction(&nvram->trans, op, &hdr);
	pr_info("nvram: active: %s\n", active_str(nvram->trans.active));
	nvram->list_updated = 0;

	r = 0;
exit:
	if (buf)
		free(buf);
	return r;
}

static int is_printable_string(const uint8_t* buf, uint32_t size)
{
	if (strnlen((const char*) buf, size) != size - 1) {
		return 0;
	}
	for (uint32_t i = 0; i < size - 1; ++i) {
		if (!isprint(buf[i]) && !isblank(buf[i])) {
			return 0;
		}
	}
	return 1;
}

char *nvram_get(const char* varname)
{
	if (!varname || !nvram)
		return NULL;
	struct libnvram_entry *entry = libnvram_list_get(nvram->list, (uint8_t*) varname, strlen(varname) + 1);
	if (entry && is_printable_string(entry->value, entry->value_len))
		return (char*) entry->value;
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
	if (!nvram) {
		pr_err("nvram not loaded\n");
		return 1;
	}
	if (!varname)
		return 1;

	if (!is_printable_string((uint8_t*) varname, strlen(varname) + 1)) {
		pr_err("nvram: varname not printable\n");
		return 1;
	}
	const char *var_prefix= "SYS_";
	if (!starts_with(varname, var_prefix)) {
		pr_err("nvram: varname not prefixed with %s\n", var_prefix);
		return 1;
	}
	if (!value || !strlen(value)) {
		if (libnvram_list_remove(&nvram->list, (uint8_t*) varname, strlen(varname) + 1)) {
			nvram->list_updated = 1;
		}
		return 0;
	}
	if (!is_printable_string((uint8_t*) value, strlen(value) + 1)) {
		pr_err("nvram: value not printable\n");
		return 1;
	}

	const char *_val = nvram_get(varname);
	if (_val && !strcmp(_val, value)) {
		// already equal
		return 0;
	}
	struct libnvram_entry entry;
	entry.key = (uint8_t*) varname;
	entry.key_len = strlen(varname) + 1;
	entry.value = (uint8_t*) value;
	entry.value_len = strlen(value) + 1;
	int r = libnvram_list_set(&nvram->list, &entry);
	if (r) {
		pr_err("nvram list_set libnvram_error: %d\n", r);
		return 1;
	}

	nvram->list_updated = 1;
	return 0;
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

struct libnvram_list* const nvram_get_list(void)
{
	if (!nvram) {
		return NULL;
	}
	return nvram->list;
}
