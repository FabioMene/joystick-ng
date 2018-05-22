KDIR = /lib/modules/$(shell uname -r)/build

TARGET = joystick_ng

joystick_ng-objs := joystick-ng-core.o joystick-ng-control-fops.o joystick-ng-driver-fops.o joystick-ng-client-fops.o list.o

obj-m += joystick_ng.o

all:
	make -C $(KDIR) M='$(shell pwd)' modules

install:
	make INSTALL_MOD_DIR=kernel/drivers/input -C $(KDIR) M='$(shell pwd)' modules_install
	depmod -A

clean:
	make -C $(KDIR) M='$(shell pwd)' clean

uninstall:
	echo Module Uninstall TODO
