# Makefile for custom_qemu_device_driver kernel module

obj-m += custom_qemu_device_driver.o

# Default kernel source (can be overridden)
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean

install:
	make -C $(KERNEL_SRC) M=$(PWD) modules_install

help:
	@echo "Targets:"
	@echo "  all     - Build the kernel module"
	@echo "  clean   - Remove built files"
	@echo "  install - Install module to system"
	@echo ""
	@echo "Variables:"
	@echo "  KERNEL_SRC - Path to kernel source (default: /lib/modules/\$$(uname -r)/build)"
	@echo ""
	@echo "Example cross-compile:"
	@echo "  make KERNEL_SRC=/path/to/kernel ARCH=x86_64 CROSS_COMPILE=x86_64-linux-gnu-"

.PHONY: all clean install help