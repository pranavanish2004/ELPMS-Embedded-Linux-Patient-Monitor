# Makefile for the ELPMS heart-rate kernel module.
#
# Kbuild syntax: "obj-m" lists the modules to build. heart_rate_driver.o
# is built from heart_rate_driver.c and becomes heart_rate_driver.ko.
obj-m += heart_rate_driver.o

# Where the kernel build tree lives.
#  - Normal Ubuntu / VM: /lib/modules/<running kernel>/build  (from the
#    linux-headers-* package)
#  - WSL2: Microsoft's kernel has no such package, so we build against the
#    WSL2-Linux-Kernel source tree you prepared in ~/WSL2-Linux-Kernel.
# Override at any time with:  make KDIR=/some/path
KDIR_DEFAULT := /lib/modules/$(shell uname -r)/build
ifneq ($(wildcard $(KDIR_DEFAULT)/Makefile),)
KDIR ?= $(KDIR_DEFAULT)
else
KDIR ?= $(HOME)/WSL2-Linux-Kernel
endif

# A kernel tree prepared only with "make modules_prepare" (the quick WSL2
# route) has no Module.symvers file, and without it the final link check
# (modpost) aborts with "undefined symbol" errors. Downgrade those to
# warnings in that case; the symbols are resolved by the running kernel at
# insmod time anyway. A full build or a distro headers package has the file,
# so this flag is then not used.
ifeq ($(wildcard $(KDIR)/Module.symvers),)
MODPOST_FLAGS := KBUILD_MODPOST_WARN=1
endif

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) $(MODPOST_FLAGS) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
