all:
	make -C ds3
	make -C ds3bt
	make -C x360usb

install:
	mkdir -p /usr/local/share/jng/drivers/
	make -C ds3 install
	make -C ds3bt install
	make -C x360usb install

clean:
	make -C ds3 clean
	make -C ds3bt clean
	make -C x360usb clean

uninstall:
	make -C ds3 uninstall
	make -C ds3bt uninstall
	make -C x360usb uninstall
