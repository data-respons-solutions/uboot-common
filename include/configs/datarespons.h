#ifndef __DATARESPONS_CONFIG_H
#define __DATARESPONS_CONFIG_H

#define BOOTSCRIPT_NOSECURE \
	"run setargs; run loadfdt;" \
	"if run loadimage; then " \
		"bootz ${loadaddr} - ${fdt_addr};" \
	"else " \
		"echo ERROR: Could not load prescribed config;" \
	"fi;"

#define BOOTSCRIPT_SECURE \
	"run setargs; " \
	"if run load_ivt_info; then " \
		"echo IVT starts at ${ivt_offset}; " \
		"if run loadimage; then " \
			"if hab_auth_img ${loadaddr} ${filesize} ${ivt_offset}; then " \
				"echo Authenticated kernel; " \
				"run loadfdt; bootz ${loadaddr} - ${fdt_addr}; " \
			"else " \
				"echo Failed to authenticate kernel; " \
			"fi; " \
		"else " \
			"echo Failed to load image ${zimage}; " \
		"fi; " \
	"else " \
		"echo No IVT information; " \
	"fi;"

#define VALIDATE_ZIMAGE \
	"if run load_ivt_info; then " \
		"echo kernel IVT starts at ${ivt_offset}; " \
		"if run loadimage; then " \
			"if hab_auth_img ${loadaddr} ${filesize} ${ivt_offset}; then " \
				"echo Authenticated kernel; " \
			"else " \
				"echo Failed to authenticate kernel && false;" \
			"fi; " \
		"else " \
			"echo Failed to load image ${zimage} && false; " \
		"fi; " \
	"else " \
		"echo No IVT information && false; " \
	"fi;"

#define VALIDATE_INITRD \
	"if run load_initrd_ivt_info; then " \
		"echo INITRD IVT loaded at ${initrd_addr} offset is ${ivt_offset}; " \
		"if run loadinitrd; then " \
			"if hab_auth_img ${initrd_addr} ${filesize} ${ivt_offset}; then " \
				"echo Authenticated initrd; " \
			"else " \
				"echo Failed to authenticate initrd && false; " \
			"fi; " \
		"else " \
			"echo Failed to load image ${initrd_file} && false; " \
		"fi; " \
	"else " \
		"echo No IVT information && false; " \
	"fi;"

#define BOOT_PRELOADED \
	"echo trying preloaded boot...;" \
	"run setpreloaded;" \
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

#if defined(CONFIG_SECURE_BOOT)
#define __SECURE_BOOT_ENVIRONMENT \
	"bootscript_secure="BOOTSCRIPT_SECURE"\0" \
	"ivt_offset=0\0" \
	"load_ivt_info=if ext4load ${bootfrom} ${bootdev}:${bootpart} 11F00000 /boot/zImage-padded-size; then env import -t 11F00000 ${filesize}; fi; \0" \
	"load_initrd_ivt_info=if ext4load ${bootfrom} ${bootdev}:${bootpart} 11F00000 /boot/initrd-padded-size; then env import -t 11F00000 ${filesize}; fi; \0" \
	"validate_image=" VALIDATE_ZIMAGE "\0" \
	"validate_initrd=" VALIDATE_INITRD "\0"
#else
#define __SECURE_BOOT_ENVIRONMENT
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
	"setpreloaded=setenv bootargs rdinit=/linuxrc enable_wait_mode=off \0" \
	"setargs=setenv bootargs root=${rootdev} rootwait ro rootfstype=ext4 consoleblank=${consoleblank} loglevel=${loglevel}\0" \
	"loadimage=ext4load ${bootfrom} ${bootdev}:${bootpart} ${loadaddr} ${zimage}; \0" \
	"loadinitrd=ext4load ${bootfrom} ${bootdev}:${bootpart} ${initrd_addr} ${initrd_file}; \0" \
	"loadfdt=ext4load ${bootfrom} ${bootdev}:${bootpart} ${fdt_addr} ${fdt_file}; \0" \
	"bootpreloaded="BOOT_PRELOADED"\0" \
	"loadbootscript=if ext4load ${bootfrom} ${bootdev}:${bootpart} ${loadaddr} /boot/boot.txt; then env import -t ${loadaddr} ${filesize}; fi; \0" \
	"bootscript="BOOTSCRIPT_NOSECURE"\0" \
	__SECURE_BOOT_ENVIRONMENT \
	"loglevel="DEFAULT_LOGLEVEL"\0" \
	"consoleblank="DEFAULT_CONSOLEBLANK"\0"

#endif /* __DATARESPONS_CONFIG_H */
