all:
	make -C ds3
	make -C ds3bt

install:
	mkdir -p /usr/local/share/jng/drivers/
	make -C ds3 install
	make -C ds3bt install

clean:
	make -C ds3 clean
	make -C ds3bt clean

uninstall:
	make -C ds3 uninstall
	make -C ds3bt uninstall
