all:
	make -C jngd
	make -C libjngd-client
	
	gcc -Wall jngtest.c -o jngtest
	gcc -Wall jngctl.c -o jngctl -Llibjngdsett -ljngdsett

install:
	make -C jngd install
	make -C libjngd-client install
	
	install -m 755 jngtest /usr/local/bin/
	install -m 755 jngctl /usr/local/bin/
	
	install -m 644 50-joystick-ng.rules /etc/udev/rules.d/
	make -C systemd install

clean:
	make -C jngd clean
	make -C libjngd-client clean
	
	rm -f jngtest jngctl

uninstall:
	make -C jngd uninstall
	make -C libjngd-client uninstall
	
	rm -f /usr/local/bin/jngtest
	rm -f /usr/local/bin/jngctl
	
	make -C systemd uninstall
