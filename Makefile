ifeq ($(CONFIG_SPL_BUILD),y)
obj- := __dummy__.o
obj-$(CONFIG_SPL_LIBNVRAM) += libnvram/libnvram.o libnvram/crc32.o
else
libnvram-y := libnvram/libnvram.o libnvram/crc32.o
obj-$(CONFIG_DR_NVRAM) += nvram/nvram.o libnvram.o
obj-$(CONFIG_CMD_DR_NVRAM) += nvram/nvram_cmd.o
obj-$(CONFIG_DR_NVRAM_BOOTSPLASH) += nvram/bootsplash.o
obj-$(CONFIG_DR_BOARD_LATE_INIT) += common/board_late_init.o
endif
