ifeq ($(CONFIG_SPL_BUILD),y)
obj- := __dummy__.o
else
libnvram-y := libnvram/libnvram.o libnvram/crc32.o
obj-$(CONFIG_DR_NVRAM) += nvram/nvram.o libnvram.o
endif
