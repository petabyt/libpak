#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <dbus/dbus.h>
#include "dbus.h"

int list_bluez(void) {
	DBusConnection *conn = get_dbus_system();

	DBusMessage *resp = send_message_noargs(conn, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
	if (resp == NULL) return -1;

	DBusMessageIter iter;
	dbus_message_iter_init(resp, &iter);
	int type;

	if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
		DBusMessageIter dict;
		dbus_message_iter_recurse(&iter, &dict);
		if (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
			DBusMessageIter dict_entry;
			dbus_message_iter_recurse(&dict, &dict_entry);
			printf("%d\n", dbus_message_iter_get_arg_type(&dict_entry));
			if (dbus_message_iter_get_arg_type(&dict_entry) == DBUS_TYPE_OBJECT_PATH) {
				const char *path = NULL;
				dbus_message_iter_get_basic(&dict_entry, &path);
				if (path) {
					printf("Path: %s\n", path);
				}
			}
		}
	}
	return 0;
}

int test_bluetooth(void) {
	int fd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	if (fd < 0) return -1;
	close(fd);
	return 0;
}

int test_wifi(void);

int main(void) {
	return test_wifi();
	return 0;
}
