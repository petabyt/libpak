// dbus bluez impl
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <dbus/dbus.h>
#include "dbus.h"
#include "bluetooth.h"

static int get_bluez_bool_property(DBusConnection *conn, const char *path, const char *iface, const char *prop, dbus_bool_t *v) {
	DBusMessage *resp;
	int rc = get_dbus_property(conn, "org.bluez", path, iface, prop, &resp);

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
    dbus_message_iter_recurse(&args, &subargs);
    dbus_message_iter_get_basic(&subargs, v);

    dbus_message_unref(resp);

	return 0;
}

static int get_bluez_string_property(DBusConnection *conn, const char *path, const char *iface, const char *prop, const char **v) {
	DBusMessage *resp;
	int rc = get_dbus_property(conn, "org.bluez", path, iface, prop, &resp);

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
    dbus_message_iter_recurse(&args, &subargs);
    dbus_message_iter_get_basic(&subargs, v);

    dbus_message_unref(resp);

	return 0;
}

static int is_hc_powered(DBusConnection *conn, const char *path) {
	dbus_bool_t v;
	get_bluez_bool_property(conn, "/org/bluez/hci0", "org.bluez.Adapter1", "Powered", &v);
	return v;
}

static int find_dict(struct DBusMessageIter *dict_iter, const char *name_arg, struct DBusMessageIter *dict) {
	int len = dbus_message_iter_get_element_count(dict_iter);
	DBusMessageIter arr_iter;
	dbus_message_iter_recurse(dict_iter, &arr_iter);
	for (int i = 0; i < len; i++) {
		dbus_message_iter_recurse(&arr_iter, dict);
		const char *name = NULL;
		dbus_message_iter_get_basic(dict, &name);
		if (name == NULL) abort();
		if (!strcmp(name, name_arg)) {
			dbus_message_iter_next(dict);
			return 0;
		}
		dbus_message_iter_next(&arr_iter);
	}
	return -1;
}

static int fill_adapter_from_dict(struct DBusMessageIter *dict_iter, struct PakBtAdapter *adapter) {
	int len = dbus_message_iter_get_element_count(dict_iter);
	DBusMessageIter arr_iter;
	dbus_message_iter_recurse(dict_iter, &arr_iter);
	for (int i = 0; i < len; i++) {
		struct DBusMessageIter dict;
		dbus_message_iter_recurse(&arr_iter, &dict);
		const char *name = NULL;
		dbus_message_iter_get_basic(&dict, &name);
		if (name == NULL) abort();
		dbus_message_iter_next(&dict);
		struct DBusMessageIter dict_variant;
		dbus_message_iter_recurse(&dict, &dict_variant);

		if (!strcmp(name, "Powered")) {
			//printf("%c\n", dbus_message_iter_get_arg_type(&dict_variant));
			dbus_bool_t v;
			dbus_message_iter_get_basic(&dict_variant, &v);
			adapter->powered = v;
		} else if (!strcmp(name, "Alias")) {
			const char *v;
			dbus_message_iter_get_basic(&dict_variant, &v);
			strlcpy(adapter->name, v, sizeof(adapter->name));
		} else if (!strcmp(name, "Address")) {
			const char *v;
			dbus_message_iter_get_basic(&dict_variant, &v);
			strlcpy(adapter->address, v, sizeof(adapter->address));
		}

		dbus_message_iter_next(&arr_iter);
	}
	return -1;
}

int pak_bt_get_adapters(struct PakBt *ctx, struct PakBtAdapterList **list_arg) {
	DBusConnection *conn = get_dbus_system();

	DBusMessage *resp = send_message_noargs(conn, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
	if (resp == NULL) return -1;

	DBusMessageIter iter;
	dbus_message_iter_init(resp, &iter);

	int len = dbus_message_iter_get_element_count(&iter);
	struct PakBtAdapterList *list = malloc(sizeof(struct PakBtAdapterList) + (sizeof(struct PakBtAdapter) * len));
	list->length = 0;

	DBusMessageIter iter_arr;
	dbus_message_iter_recurse(&iter, &iter_arr);

	for (int i = 0; i < len; i++) {
		DBusMessageIter dict;
		dbus_message_iter_recurse(&iter_arr, &dict);

		const char *path = NULL;
		dbus_message_iter_get_basic(&dict, &path);
		if (path == NULL) abort();

		dbus_message_iter_next(&dict);
		DBusMessageIter adapter_dict;
		if (!find_dict(&dict, "org.bluez.Adapter1", &adapter_dict)) {
			//list->list[list->length].priv = path; // todo: strdup?
			fill_adapter_from_dict(&adapter_dict, &list->list[list->length]);
			list->length++;
		}

		dbus_message_iter_next(&iter_arr);
	}

	dbus_message_unref(resp);

	(*list_arg) = list;
	return 0;
}

int test_bluetooth(void) {
	DBusConnection *conn = get_dbus_system();

//	const char *v = NULL;
//	get_bluez_string_property(conn, "/org/bluez/hci0", "org.bluez.Adapter1", "Address", &v);
//	printf("%s\n", v);

	//printf("Is hc powered: %d\n", is_hc_powered(conn, "/org/bluez/hci0"));

//	list_bluez();
//	int fd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
//	if (fd < 0) return -1;
//	close(fd);
	return 0;
}
