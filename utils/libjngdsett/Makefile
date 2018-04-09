all:
	gcc -Wall -c -fPIC jngdsett.c -o jngdsett.o
	gcc -Wall -shared -fPIC -o libjngdsett.so jngdsett.o -lc

install:
	install -m 755 libjngdsett.so /usr/local/lib/
	install -m 644 libjngdsett.h  /usr/local/include/
	ldconfig
	mkdir -p /etc/jngd/defs
	mkdir -p /etc/jngd/settings

clean:
	rm -f jngdsett.o libjngdsett.so

uninstall:
	rm -f /usr/local/lib/libjngdsett.so
	rm -f /usr/local/include/libjngdsett.h
