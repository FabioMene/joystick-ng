all:
	gcc -Wall main.c -o x360usbdrv -L../../utils/libjngd-client -ljngd-client -lusb

install:
	install -m 755 x360usbdrv /usr/local/share/jng/drivers/x360usb
	install -m 644 x360usb.def /etc/jngd/defs/x360usb
	install -m 644 50-jng-x360usb.rules /etc/udev/rules.d/

clean:
	rm -f x360usbdrv

uninstall:
	rm -f /usr/local/share/jng/drivers/x360usb
	rm -f /etc/jngd/defs/x360usb
	rm -f /etc/udev/rules.d/50-jng-x360usb.rules
