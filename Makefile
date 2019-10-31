ifeq ($(CONFIG_SPL_BUILD),y)
obj- := __dummy__.o
else
obj-$(CONFIG_DR_NVRAM) += nvram/nvram.o
endif
