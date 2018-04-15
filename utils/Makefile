all:
	gcc -Wall jngtest.c -o jngtest
	gcc -Wall jngdriverd.c -o jngdriverd
	make -C libjngdsett
	gcc -Wall jngdctl.c -o jngdctl -Llibjngdsett -ljngdsett

install:
	install -m 755 jngtest /usr/local/bin/
	install -m 755 jngdriverd /usr/local/bin/
	install -m 644 50-joystick-ng.rules /etc/udev/rules.d/
	make -C libjngdsett install
	install -m 755 jngdctl /usr/local/bin/
	make -C systemd install

clean:
	rm -f jngtest jngdriverd
	make -C libjngdsett clean
	rm -f jngdctl

uninstall:
	rm -f /usr/local/bin/jngtest
	rm -f /usr/local/bin/jngdriverd
	make -C libjngdsett uninstall
	rm -f /usr/local/bin/jngdctl
	make -C systemd uninstall
