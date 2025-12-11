all:
	gcc dbus.c wifi.c `pkg-config --cflags --libs bluez dbus-1` -o a.out
	./a.out
