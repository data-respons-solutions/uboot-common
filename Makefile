ifeq ($(CONFIG_SPL_BUILD),y)
obj- := __dummy__.o
else
libnvram-y := libnvram/libnvram.o libnvram/crc32.o
obj-$(CONFIG_DR_NVRAM) += nvram/nvram.o libnvram.o
obj-$(CONFIG_CMD_DR_NVRAM) += nvram/nvram_cmd.o
obj-$(CONFIG_DR_BOOTSPLASH) += bootsplash/bootsplash.o
endif
