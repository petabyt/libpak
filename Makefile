all:
	gcc test.c wifi.c bluez.c `pkg-config --cflags --libs dbus-1` `pkg-config --cflags bluez` -o a.out
	./a.out
