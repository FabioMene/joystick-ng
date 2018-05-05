all:
	gcc -Wall -Wextra -c -fPIC -o jngd-client.o jngd-client.c 
	gcc -Wall -Wextra -shared -o libjngd-client.so jngd-client.o -lc

install:
	install -m 755 libjngd-client.so /usr/local/lib/
	install -m 644 libjngd.h /usr/local/include/
	ldconfig

clean:
	rm -f *.o *.so

uninstall:
	rm -f /usr/local/lib/libjngd-client.so
	rm -f /usr/local/include/libjngd.h
	ldconfig
