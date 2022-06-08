ifeq ($(CONFIG_SPL_BUILD),y)
obj- := __dummy__.o
obj-$(CONFIG_SPL_LIBNVRAM) += libnvram/libnvram.o libnvram/crc32.o
else
libnvram-y := libnvram/libnvram.o libnvram/crc32.o
obj-$(CONFIG_DR_NVRAM) += nvram.o libnvram.o
obj-$(CONFIG_CMD_DR_NVRAM) += nvram_cmd.o
endif
