EMULATION_KERNEL=0
MODULE_EXTRA_FLAGS=""
ifneq ($(KERNELRELEASE),)
	obj-m := DMGturret.o
else
	ifeq ($(EMULATION_KERNEL), 0)
		KERNELDIR := $(EC535)/gumstix/oe/linux-2.6.21
 	else
		MODULE_EXTRA_FLAGS += "-DSIM_MODE"
		KERNELDIR := $(EC535)/gumstix/linux-2.6.21gum
	endif
	PWD := $(shell pwd)
	ARCH := arm
	CROSS := arm-linux-

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) EXTRA_CFLAGS="$(MODULE_EXTRA_FLAGS)" CROSS_COMPILE=$(CROSS) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) clean

endif
