install:
	install -m644 joystick-ng.conf /etc/modules-load.d/
	install -m644 joystick-ng.service /etc/systemd/system/
	systemctl daemon-reload
	systemctl enable joystick-ng

uninstall:
	rm -f /etc/modules-load.d/joystick-ng.conf
	rm -f /etc/systemd/system/joystick-ng.service
	systemctl daemon-reload
