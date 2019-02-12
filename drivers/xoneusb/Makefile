all:
	gcc -Wall main.c -o xoneusbdrv -L../../utils/libjngd-client -ljngd-client -lusb

install:
	install -m 755 xoneusbdrv /usr/local/share/jng/drivers/xoneusb
	install -m 644 xoneusb.def /etc/jngd/defs/xoneusb
	install -m 644 50-jng-xoneusb.rules /etc/udev/rules.d/

clean:
	rm -f xoneusbdrv

uninstall:
	rm -f /usr/local/share/jng/drivers/xoneusb
	rm -f /etc/jngd/defs/xoneusb
	rm -f /etc/udev/rules.d/50-jng-xoneusb.rules
