all:
	gcc -Wall main.c -o ds3btdrv -L../../utils/libjngdsett -ljngdsett -lbluetooth -pthread

install:
	install -m 755 ds3btdrv /usr/local/share/jng/drivers/ds3bt
	install -m 644 ds3bt.def /etc/jngd/defs/ds3bt
	install -m 644 50-jng-ds3bt.rules /etc/udev/rules.d/
	install -m 755 ds3bt_start /usr/local/bin/
	install -m 644 jng-ds3bt-starter.service /etc/systemd/system
	systemctl daemon-reload
	systemctl enable jng-ds3bt-starter

clean:
	rm -f ds3btdrv

uninstall:
	rm -f /usr/local/share/jng/drivers/ds3bt
	rm -f /etc/jngd/defs/ds3bt
	rm -f /etc/udev/rules.d/50-jng-ds3bt.rules
	rm -f /usr/local/bin/ds3bt_start
	rm -f /etc/systemd/system/jng-ds3bt-starter.service
	systemctl daemon-reload
