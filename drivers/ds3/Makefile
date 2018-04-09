all:
	gcc main.c -o ds3drv -L../../utils/libjngdsett -ljngdsett -lusb

install:
	install -m 755 ds3drv /usr/local/share/jng/drivers/ds3
	install -m 644 ds3.def /etc/jngd/defs/ds3
	install -m 644 50-jng-ds3.rules /etc/udev/rules.d/

clean:
	rm -f ds3drv

uninstall:
	rm -f /usr/local/share/jng/drivers/ds3
	rm -f /etc/jngd/defs/ds3
	rm -f /etc/udev/rules.d/50-jng-ds3.rules
