all:
	gcc -Wall main.c -o ds3btdrv -L../../utils/libjngd-client -ljngd-client -lbluetooth -pthread

install:
	install -m 755 ds3btdrv /usr/local/share/jng/drivers/ds3bt
	install -m 644 ds3bt.def /etc/jngd/defs/ds3bt
	
	install -m 755 jng-ds3bt-helper /usr/local/bin/
	
	install -m 644 systemd/jng-ds3bt-helper-beforebt.service    /etc/systemd/system
	install -m 644 systemd/jng-ds3bt-helper-afterbt.service     /etc/systemd/system
	install -m 644 systemd/jng-ds3bt-helper-onbtstop.service    /etc/systemd/system
	install -m 644 systemd/jng-ds3bt-helper-afterresume.service /etc/systemd/system
	
	systemctl daemon-reload
	systemctl enable jng-ds3bt-helper-beforebt
	systemctl enable jng-ds3bt-helper-afterbt
	systemctl enable jng-ds3bt-helper-onbtstop
	systemctl enable jng-ds3bt-helper-afterresume

clean:
	rm -f ds3btdrv

uninstall:
	rm -f /usr/local/share/jng/drivers/ds3bt
	rm -f /etc/jngd/defs/ds3bt
	rm -f /usr/local/bin/jng-ds3bt-helper
	
	rm -f /etc/systemd/system/jng-ds3bt-helper-beforebt.service
	rm -f /etc/systemd/system/jng-ds3bt-helper-afterbt.service
	rm -f /etc/systemd/system/jng-ds3bt-helper-onbtstop.service
	rm -f /etc/systemd/system/jng-ds3bt-helper-afterresume.service
	systemctl daemon-reload
