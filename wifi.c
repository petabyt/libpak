#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <dbus/dbus.h>
#include "dbus.h"
#include "wifi.h"

struct WiFiContext {
	DBusConnection *conn;
};

int get_networkmanager_basic_property(DBusConnection *conn, const char *path, const char *iface, const char *prop, void *val) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", path, "org.freedesktop.DBus.Properties", "Get");
    dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);
	DBusMessage *resp = dbus_connection_send_with_reply_and_block(conn, call, DBUS_TIMEOUT_USE_DEFAULT, &error);
	dbus_message_unref(call);
	if (resp == NULL) return -1;
	if (dbus_error_is_set(&error)) return -1;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
    dbus_message_iter_recurse(&args, &subargs);
    dbus_message_iter_get_basic(&subargs, val);

    return 0;
}

int get_networkmanager_u8array_property(DBusConnection *conn, const char *path, const char *iface, const char *prop, void *val) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", path, "org.freedesktop.DBus.Properties", "Get");
    dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);
	DBusMessage *resp = dbus_connection_send_with_reply_and_block(conn, call, DBUS_TIMEOUT_USE_DEFAULT, &error);
	dbus_message_unref(call);
	if (resp == NULL) return -1;
	if (dbus_error_is_set(&error)) return -1;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
    dbus_message_iter_recurse(&args, &subargs);

	int len = dbus_message_iter_get_element_count(&subargs);
	char *s = malloc(len + 1);

	if (dbus_message_iter_get_arg_type(&subargs) == DBUS_TYPE_ARRAY) {
		DBusMessageIter dict;
		dbus_message_iter_recurse(&subargs, &dict);
		int current_type;
		int i = 0;
		while ((current_type = dbus_message_iter_get_arg_type(&dict)) != DBUS_TYPE_INVALID) {
			uint8_t c;
			dbus_message_iter_get_basic(&dict, &c);
			s[i] = (char)c;
			dbus_message_iter_next(&dict);
			i++;
		}
		s[i] = '\0';
	}

	((char **)val)[0] = s;

    return 0;
}

int get_networkmanager_service_basic_property(DBusConnection *conn, const char *prop, void *val) {
	get_networkmanager_basic_property(conn, "/org/freedesktop/NetworkManager", "org.freedesktop.NetworkManager", prop, val);
    return 0;
}

int get_networkmanager_active_connections(DBusConnection *conn) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager", "org.freedesktop.DBus.Properties", "Get");

	const char *iface = "org.freedesktop.NetworkManager";
	const char *prop = "ActiveConnections";
    dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);
	DBusMessage *resp = dbus_connection_send_with_reply_and_block(conn, call, DBUS_TIMEOUT_USE_DEFAULT, &error);
	dbus_message_unref(call);
	if (resp == NULL) return -1;
	if (dbus_error_is_set(&error)) return -1;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
    dbus_message_iter_recurse(&args, &subargs);

	if (dbus_message_iter_get_arg_type(&subargs) == DBUS_TYPE_ARRAY) {
		DBusMessageIter dict;
		dbus_message_iter_recurse(&subargs, &dict);
		int current_type;
		while ((current_type = dbus_message_iter_get_arg_type(&dict)) != DBUS_TYPE_INVALID) {
			if (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_OBJECT_PATH) {
				const char *path = NULL;
				dbus_message_iter_get_basic(&dict, &path);
				if (path) {
					printf("active connection: %s\n", path);
				}
			}
			dbus_message_iter_next (&dict);
		}
	}

    return 0;
}

int get_networkmanager_connection_path(DBusConnection *conn, const char *path) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", path, "org.freedesktop.DBus.Properties", "Get");

	const char *iface = "org.freedesktop.NetworkManager.Connection.Active";
	const char *prop = "Connection";
    dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);
	DBusMessage *resp = dbus_connection_send_with_reply_and_block(conn, call, DBUS_TIMEOUT_USE_DEFAULT, &error);
	dbus_message_unref(call);
	if (resp == NULL) {
		printf("call fail\n");
		return -1;
	}
	if (dbus_error_is_set(&error)) return -1;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
    dbus_message_iter_recurse(&args, &subargs);

	if (dbus_message_iter_get_arg_type(&subargs) == DBUS_TYPE_OBJECT_PATH) {
		const char *path = NULL;
		dbus_message_iter_get_basic(&subargs, &path);
		if (path) {
			printf("connection path: %s\n", path);
		}
	}

    return 0;
}

int get_networkmanager_devices(DBusConnection *conn) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager", "org.freedesktop.DBus.Properties", "Get");

	const char *iface = "org.freedesktop.NetworkManager";
	const char *prop = "Devices";
    dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_INVALID);
    dbus_message_append_args(call, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

	DBusMessage *resp = dbus_connection_send_with_reply_and_block(conn, call, DBUS_TIMEOUT_USE_DEFAULT, &error);
	dbus_message_unref(call);
	if (resp == NULL) return -1;
	if (dbus_error_is_set(&error)) return -1;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
    dbus_message_iter_recurse(&args, &subargs);

	if (dbus_message_iter_get_arg_type(&subargs) == DBUS_TYPE_ARRAY) {
		DBusMessageIter dict;
		dbus_message_iter_recurse(&subargs, &dict);
		int current_type;
		while ((current_type = dbus_message_iter_get_arg_type(&dict)) != DBUS_TYPE_INVALID) {
			if (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_OBJECT_PATH) {
				const char *path = NULL;
				dbus_message_iter_get_basic(&dict, &path);
				if (path) {
					printf("device: %s\n", path);
					const char *dev_iface = NULL;
					get_networkmanager_basic_property(conn, path, "org.freedesktop.NetworkManager.Device", "Interface", &dev_iface);
					printf("interf: %s\n", dev_iface);
					uint64_t dev_type = 0;
					get_networkmanager_basic_property(conn, path, "org.freedesktop.NetworkManager.Device", "DeviceType", &dev_type);
					printf("interf: %lu\n", dev_type);
				}
			}
			dbus_message_iter_next (&dict);
		}
	}

    return 0;
}

int wifi_get_ap_list(struct WiFiContext *ctx, struct WiFiApList **ap_list) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager/Devices/4", "org.freedesktop.DBus.Properties", "Get");

	const char *iface = "org.freedesktop.NetworkManager.Device.Wireless";
	const char *prop = "AccessPoints";
    dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

	DBusMessage *resp = send_message(ctx->conn, call);
	if (resp == NULL) return -1;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
    dbus_message_iter_recurse(&args, &subargs);

	if (dbus_message_iter_get_arg_type(&subargs) != DBUS_TYPE_ARRAY) return -1;

	int len = dbus_message_iter_get_element_count(&subargs);
	struct WiFiApList *list = malloc(sizeof(struct WiFiApList) + (sizeof(struct WiFiAp) * len));
	list->length = len;

	DBusMessageIter dict;
	dbus_message_iter_recurse(&subargs, &dict);

	int current_type;
	for (int i = 0; i < len; i++) {
		if (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_OBJECT_PATH) return -1;
		const char *path = NULL;
		dbus_message_iter_get_basic(&dict, &path);
		const char *s = NULL;
		get_networkmanager_u8array_property(ctx->conn, path, "org.freedesktop.NetworkManager.AccessPoint", "Ssid", &s);
		strlcpy(list->list[i].ssid, s, sizeof(list->list[i].ssid));

		dbus_message_iter_next(&dict);
	}

	dbus_message_unref(resp);

	(*ap_list) = list;

    return 0;
}

int wifi_is_enabled(void) {
	DBusConnection *conn = get_dbus_system();
	dbus_bool_t v;
	get_networkmanager_service_basic_property(conn, "WirelessEnabled", &v);
	return v;
}

int test_wifi(void) {

	struct WiFiContext ctx = {
		.conn = get_dbus_system(),
	};

	struct WiFiApList *aps;

	if (wifi_get_ap_list(&ctx, &aps)) return -1;

	for (int i = 0; i < aps->length; i++) {
		printf("'%s'\n", aps->list[i].ssid);
	}

	return 0;
}
