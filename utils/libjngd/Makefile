all:
	# Libreria client
	gcc -g3 -Wall -Wextra -c -fPIC -o jngd-client.o -DJNGD_CLIENT jngd-client.c 
	gcc -g3 -Wall -Wextra -shared -o libjngd.so jngd-client.o -lc

	gcc -g3 -Wall -Wextra -o test test.c -L. -ljngd
