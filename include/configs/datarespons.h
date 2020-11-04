#ifndef __DATARESPONS_CONFIG_H
#define __DATARESPONS_CONFIG_H

#define BOOTSCRIPT \
	"run setargs; run loadfdt;" \
	"if run loadimage; then " \
		"bootz ${loadaddr} - ${fdt_addr};" \
	"else " \
		"echo ERROR: Could not load prescribed config;" \
	"fi;"

#define BOOT_PRELOADED \
	"echo trying preloaded boot...;" \
	"bootz ${loadaddr} ${initrd_addr} ${fdt_addr};" \
	"echo    no preloaded image;"

#define BOOT_USB \
	"echo trying usb boot...;" \
	"if usb start; then " \
		"if usb storage; then " \
			"if part uuid usb ${usbdev}:${usbpart} partuuid; then " \
				"run setusb;" \
				"run loadbootscript;" \
				"run bootscript;" \
				"echo USB boot failed;" \
			"else " \
				"echo failed getting usb part uuid;" \
			"fi;" \
		"else " \
			"echo no usb device available;" \
		"fi;" \
	"else " \
		"echo could not start usb;" \
	"fi;"

#define BOOT_MMC \
	"echo trying mmc boot...;" \
	"if mmc dev ${mmcdev}; then " \
		"if part uuid mmc ${mmcdev}:${mmcpart} partuuid; then " \
			"run setmmc;" \
			"run loadbootscript;" \
			"run bootscript;" \
			"echo mmc boot failed;" \
		"else " \
			"echo failed gettinc mmc part uuid;" \
		"fi;" \
	"else " \
		"echo no mmc device available;" \
	"fi;"

#if defined(CONFIG_CMD_MMC)
#define __MMC_ENVIRONMENT \
	"mmcdev="DEFAULT_MMC_DEV"\0" \
	"mmcpart="DEFAULT_MMC_PART"\0" \
	"setmmc=setenv bootfrom mmc; setenv bootdev ${mmcdev}; setenv bootpart ${mmcpart}; setenv rootdev PARTUUID=${partuuid}; \0 " \
	"bootmmc="BOOT_MMC"\0"
#else
#define __MMC_ENVIRONMENT
#endif

#if defined(CONFIG_CMD_USB)
#define __USB_ENVIRONMENT \
	"usbdev="DEFAULT_USB_DEV"\0" \
	"usbpart="DEFAULT_USB_PART"\0" \
	"setusb=setenv bootfrom usb; setenv bootdev ${usbdev}; setenv bootpart ${usbpart}; setenv rootdev PARTUUID=${partuuid}; \0 " \
	"bootusb="BOOT_USB"\0"
#else
#define __USB_ENVIRONMENT
#endif

#define DATARESPONS_BOOT_SCRIPTS \
	"zimage="DEFAULT_ZIMAGE"\0" \
	"initrd_file="DEFAULT_INITRD"\0" \
	"initrd_addr="DEFAULT_INITRD_ADDR"\0" \
	"initrd_high=0xffffffff\0" \
	"fdt_file="DEFAULT_FDT"\0" \
	"fdt_addr="DEFAULT_FDT_ADDR"\0" \
	"fdt_high=0xffffffff\0" \
	__MMC_ENVIRONMENT \
	__USB_ENVIRONMENT \
	"setargs=setenv bootargs root=${rootdev} rootwait ro rootfstype=ext4 consoleblank=${consoleblank} loglevel=${loglevel}\0" \
	"loadimage=ext4load ${bootfrom} ${bootdev}:${bootpart} ${loadaddr} ${zimage}; \0" \
	"loadinitrd=ext4load ${bootfrom} ${bootdev}:${bootpart} ${initrd_addr} ${initrd_file}; \0" \
	"loadfdt=ext4load ${bootfrom} ${bootdev}:${bootpart} ${fdt_addr} ${fdt_file}; \0" \
	"bootpreloaded="BOOT_PRELOADED"\0" \
	"loadbootscript=if ext4load ${bootfrom} ${bootdev}:${bootpart} ${loadaddr} /boot/boot.txt; then env import -t ${loadaddr} ${filesize}; fi; \0" \
	"bootscript="BOOTSCRIPT"\0" \
	"loglevel="DEFAULT_LOGLEVEL"\0" \
	"consoleblank="DEFAULT_CONSOLEBLANK"\0"

#endif /* __DATARESPONS_CONFIG_H */
