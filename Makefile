all:
	gcc test.c wifi.c bluez.c `pkg-config --cflags --libs bluez dbus-1` -o a.out
	./a.out
