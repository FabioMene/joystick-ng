all:
	gcc -Wall main.c -o ds3btdrv -L../../utils/libjngd-client -ljngd-client -lbluetooth -pthread

install:
	install -m 755 ds3btdrv /usr/local/share/jng/drivers/ds3bt
	install -m 644 ds3bt.def /etc/jngd/defs/ds3bt
	install -m 755 jng-ds3bt-start /usr/local/bin/
	install -m 644 jng-ds3bt-starter.service /etc/systemd/system
	systemctl daemon-reload
	systemctl enable jng-ds3bt-starter

clean:
	rm -f ds3btdrv

uninstall:
	rm -f /usr/local/share/jng/drivers/ds3bt
	rm -f /etc/jngd/defs/ds3bt
	rm -f /usr/local/bin/jng-ds3bt-start
	rm -f /etc/systemd/system/jng-ds3bt-starter.service
	systemctl daemon-reload
