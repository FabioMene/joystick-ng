all:
	gcc -Wall -Wextra -c -fPIC -o jngd-client.o jngd-client.c 
	gcc -Wall -Wextra -shared -o libjngd.so jngd-client.o -lc

clean:
	rm -f *.o *.so
