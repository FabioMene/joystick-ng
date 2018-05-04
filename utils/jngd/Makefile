all:
	gcc -Wall -Wextra -c jngd.c -o jngd.o
	gcc -Wall -Wextra -c user-elevated.c -o user-elevated.o
	gcc -Wall -Wextra -c client-service.c -o client-service.o
	gcc -Wall -Wextra -c driver-ops.c -o driver-ops.o
	gcc jngd.o user-elevated.o client-service.o driver-ops.o -o jngd -pthread

install:
	mkdir -p /etc/jngd/defs
	mkdir -p /etc/jngd/settings
	
	install -m 755 jngd /usr/local/bin
	install -m 644 globals.def /etc/jngd/

clean:
	rm -f jngd *.o *.so

uninstall:
	rm -f  /usr/local/bin/jngd
	rm -rf /etc/jngd/defs
	rm -f  /etc/jngd/globals.def
