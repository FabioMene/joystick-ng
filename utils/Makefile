all:
	gcc -Wall jngtest.c -o jngtest
	
	make -C libjngdsett
	
	gcc -Wall jngd.c -o jngd -Llibjngdsett -ljngdsett
	gcc -Wall jngctl.c -o jngctl -Llibjngdsett -ljngdsett

install:
	install -m 755 jngtest /usr/local/bin/
	
	make -C libjngdsett install
	
	install -m 755 jngd /usr/local/bin/
	install -m 755 jngctl /usr/local/bin/
	
	install -m 644 50-joystick-ng.rules /etc/udev/rules.d/
	make -C systemd install

clean:
	make -C libjngdsett clean
	
	rm -f jngtest jngd jngctl

uninstall:
	rm -f /usr/local/bin/jngtest
	
	make -C libjngdsett uninstall
	
	rm -f /usr/local/bin/jngd
	rm -f /usr/local/bin/jngctl
	
	make -C systemd uninstall
