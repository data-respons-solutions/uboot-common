#ifndef __DATARESPONS_CONFIG_H
#define __DATARESPONS_CONFIG_H

#ifndef FIT_IMAGE
#define FIT_IMAGE "/boot/fitImage"
#endif

#define BOOT_PRELOADED \
	"echo trying preloaded boot...;" \
	"bootm ${fit_addr};" \
	"echo    no preloaded image;"

#define BOOT_USB \
	"echo trying usb boot...;" \
	"if usb start; then " \
		"if usb storage; then " \
			"if part uuid usb ${usbdev}:${usbpart} usbuuid; then " \
				"if ext4load usb ${usbdev}:${usbpart} ${fit_addr} ${fit_image}; then " \
					"setenv bootargs rootwait root=PARTUUID=${usbuuid};" \
					"bootm ${fit_addr};" \
					"echo USB boot failed;" \
				"fi;" \
			"else " \
				"echo failed getting usb part uuid;" \
			"fi;" \
		"else " \
			"echo no usb device available;" \
		"fi;" \
	"else " \
		"echo could not start usb;" \
	"fi;"

#define BOOT_SYS \
	"echo trying system boot...;" \
	"if ext4load ${sysiface} ${sysdev}:${syspart} ${fit_addr} ${fit_image}; then " \
		"setenv bootargs root=PARTUUID=${sysuuid};" \
		"bootm ${fit_addr};" \
	"else " \
		"echo Failed loading fit image from system;" \
	"fi;"

#if defined(CONFIG_CMD_USB)
#define __USB_ENVIRONMENT \
	"usbdev="DEFAULT_USB_DEV"\0" \
	"usbpart="DEFAULT_USB_PART"\0" \
	"bootusb="BOOT_USB"\0"
#else
#define __USB_ENVIRONMENT
#endif

#define DATARESPONS_BOOT_SCRIPTS \
	"fit_addr="FIT_ADDR"\0" \
	"fit_image="FIT_IMAGE"\0" \
	__USB_ENVIRONMENT \
	"bootpreloaded="BOOT_PRELOADED"\0" \
	"bootsys="BOOT_SYS"\0"

#endif /* __DATARESPONS_CONFIG_H */
