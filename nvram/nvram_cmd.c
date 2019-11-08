#include <common.h>
#include <command.h>
#include "../include/nvram.h"
#include "../libnvram/libnvram.h"

static int do_nvram(cmd_tbl_t *cmdtp, int flag, int argc,
			char * const argv[])
{
	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	if (strncmp(argv[1], "get", 3) == 0) {
		if (argc != 3) {
			return CMD_RET_USAGE;
		}
		const char* value = nvram_get(argv[2]);
		if (!value) {
			return CMD_RET_FAILURE;
		}
		printf("%s=%s\n", argv[2], value);
	}
	else
	if (strncmp(argv[1], "list", 4) == 0) {
		struct nvram_list* list = nvram_get_list();
		if (!list) {
			return CMD_RET_FAILURE;
		}
		struct nvram_node* cur = list->entry;
		while(cur) {
			printf("%s = %s\n", cur->key, cur->value);
			cur = cur->next;
		}
	}
	else
	if (strncmp(argv[1], "set", 3) == 0) {
		if (argc != 4) {
			return CMD_RET_USAGE;
		}
		if (nvram_set(argv[2], argv[3])) {
			return CMD_RET_FAILURE;
		}
	}
	else
	if (strncmp(argv[1], "commit", 6) == 0) {
		if (nvram_commit()) {
			return CMD_RET_FAILURE;
		}
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	nvram, 4, 1, do_nvram, "nvram interface",
	"nvram get <key>            - Read value\n"
	"nvram set <key> <value>    - Write value\n"
	"nvram list                 - List all values\n"
	"nvram commit               - Commit changes to flash\n"
	"\n"
	"Note: No changes will be made to flash before calling commit\n"
	);
