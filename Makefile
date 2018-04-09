

all: module-build utils-build drivers-build

module-build:
	make -C module

utils-build:
	make -C utils

drivers-build:
	make -C drivers

install: module-install utils-install drivers-install
	udevadm control -R

module-install:
	make -C module install
	install -m 644 include/joystick-ng.h /usr/local/include/

utils-install:
	make -C utils install

drivers-install:
	make -C drivers install

clean:
	make -C module clean
	make -C utils clean
	make -C drivers clean

uninstall: module-uninstall utils-uninstall -drivers-uninstall

module-uninstall:
	make -C module uninstall
	rm -f /usr/local/include/joystick-ng.h

utils-uninstall:
	make -C utils uninstall

drivers-uninstall:
	make -C drivers uninstall
