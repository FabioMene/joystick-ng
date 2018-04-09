all:
	gcc main.c -o ds3btdrv -L../../utils/libjngdsett -ljngdsett -lbluetooth

install:
	install -m 755 ds3btdrv /usr/local/share/jng/drivers/ds3bt
	install -m 644 ds3bt.def /etc/jngd/defs/ds3bt
	install -m 644 50-jng-ds3bt.rules /etc/udev/rules.d/

clean:
	rm -f ds3btdrv

uninstall:
	rm -f /usr/local/share/jng/drivers/ds3bt
	rm -f /etc/jngd/defs/ds3bt
	rm -f /etc/udev/rules.d/50-jng-ds3bt.rules
