// dbus NetworkManager impl
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <dbus/dbus.h>
#include <string.h>
#include "dbus.h"
#include "wifi.h"

struct PakWiFi {
	DBusConnection *conn;
};

static void *get_priv(uint64_t x) {
	return (void *)(uintptr_t)x;
}

struct PakWiFi *pak_wifi_get_context(void) {
	struct PakWiFi *ctx = malloc(sizeof(struct PakWiFi));
	ctx->conn = get_dbus_system();
	return ctx;
}

static int get_networkmanager_basic_property(DBusConnection *conn, const char *path, const char *iface, const char *prop, void *val) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", path, "org.freedesktop.DBus.Properties", "Get");
    dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

	DBusMessage *resp = send_reply_and_block(conn, call);
	if (resp == NULL) return -1;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
    dbus_message_iter_recurse(&args, &subargs);
    dbus_message_iter_get_basic(&subargs, val);

    dbus_message_unref(resp);

    return 0;
}

static int get_networkmanager_u8array_property(DBusConnection *conn, const char *path, const char *iface, const char *prop, void *val) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", path, "org.freedesktop.DBus.Properties", "Get");
    dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

	DBusMessage *resp = send_reply_and_block(conn, call);
	if (resp == NULL) return -1;

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

	dbus_message_unref(resp);

    return 0;
}

static int get_networkmanager_service_basic_property(DBusConnection *conn, const char *prop, void *val) {
	get_networkmanager_basic_property(conn, "/org/freedesktop/NetworkManager", "org.freedesktop.NetworkManager", prop, val);
    return 0;
}

int pak_wifi_get_adapter_list(struct PakWiFi *ctx, struct PakWiFiAdapterList **list_arg) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager", "org.freedesktop.DBus.Properties", "Get");

	const char *iface = "org.freedesktop.NetworkManager";
	const char *prop = "Devices";
    dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_INVALID);
    dbus_message_append_args(call, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

	DBusMessage *resp = send_reply_and_block(ctx->conn, call);
	if (resp == NULL) return -1;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
    dbus_message_iter_recurse(&args, &subargs);

	if (dbus_message_iter_get_arg_type(&subargs) != DBUS_TYPE_ARRAY) return -1;

	int len = dbus_message_iter_get_element_count(&subargs);
	struct PakWiFiAdapterList *list = malloc(sizeof(struct PakWiFiAdapterList) + (sizeof(struct PakWiFiAdapter) * len));
	list->length = 0;

	DBusMessageIter dict;
	dbus_message_iter_recurse(&subargs, &dict);
	int current_type;
	while ((current_type = dbus_message_iter_get_arg_type(&dict)) != DBUS_TYPE_INVALID) {
		const char *path = NULL;
		dbus_message_iter_get_basic(&dict, &path);
		if (path == NULL) abort();

		uint64_t dev_type = 0;
		get_networkmanager_basic_property(ctx->conn, path, "org.freedesktop.NetworkManager.Device", "DeviceType", &dev_type);

		// Network cards can be detached from NetworkManager like so:
		// nmcli dev set <dev> managed no
		dbus_bool_t is_managed = 0;
		get_networkmanager_basic_property(ctx->conn, path, "org.freedesktop.NetworkManager.Device", "Managed", &is_managed);

		if (dev_type == 2 && is_managed) { // NM_DEVICE_TYPE_WIFI
			list->list[list->length].priv = (uint64_t)(uintptr_t)strdup(path);
			const char *dev_iface = NULL;
			get_networkmanager_basic_property(ctx->conn, path, "org.freedesktop.NetworkManager.Device", "Interface", &dev_iface);
			strlcpy(list->list[list->length].name, dev_iface, sizeof(list->list[list->length].name));

			list->length++;
		}
		dbus_message_iter_next(&dict);
	}

	(*list_arg) = list;

	dbus_message_unref(resp);

    return 0;
}
int pak_wifi_free_adapter(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter_arg) {
	free(get_priv(adapter_arg->priv));
	return 0;
}
int pak_wifi_free_adapter_list(struct PakWiFi *ctx, struct PakWiFiAdapterList *list_arg) {
	for (int i = 0; i < list_arg->length; i++) {
		pak_wifi_free_adapter(ctx, &list_arg->list[i]);
	}
	free(list_arg);
	return 0;
}

static int fill_ap(DBusConnection *conn, const char *path, struct PakWiFiAp *ap) {
	char *s = NULL;
	get_networkmanager_u8array_property(conn, path, "org.freedesktop.NetworkManager.AccessPoint", "Ssid", &s);
	strlcpy(ap->ssid, s, sizeof(ap->ssid));
	free(s);
	return 0;
}

int pak_wifi_get_ap_list(struct PakWiFi *ctx, struct PakWiFiApList **ap_list) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager/Devices/4", "org.freedesktop.DBus.Properties", "Get");

	const char *iface = "org.freedesktop.NetworkManager.Device.Wireless";
	const char *prop = "AccessPoints";
    dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

	DBusMessage *resp = send_reply_and_block(ctx->conn, call);
	if (resp == NULL) return -1;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
    dbus_message_iter_recurse(&args, &subargs);

	if (dbus_message_iter_get_arg_type(&subargs) != DBUS_TYPE_ARRAY) return -1;

	int len = dbus_message_iter_get_element_count(&subargs);
	struct PakWiFiApList *list = malloc(sizeof(struct PakWiFiApList) + (sizeof(struct PakWiFiAp) * len));
	list->length = len;

	DBusMessageIter dict;
	dbus_message_iter_recurse(&subargs, &dict);

	int current_type;
	for (int i = 0; i < len; i++) {
		if (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_OBJECT_PATH) return -1;
		const char *path = NULL;
		dbus_message_iter_get_basic(&dict, &path);

		fill_ap(ctx->conn, path, &list->list[i]);

		dbus_message_iter_next(&dict);
	}

	dbus_message_unref(resp);

	(*ap_list) = list;

    return 0;
}

int pak_wifi_get_connected_ap(struct PakWiFi *ctx, struct PakWiFiAp *ap) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager/Devices/4", "org.freedesktop.DBus.Properties", "Get");

	const char *iface = "org.freedesktop.NetworkManager.Device.Wireless";
	const char *prop = "ActiveAccessPoint";
    dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

	DBusMessage *resp = send_reply_and_block(ctx->conn, call);
	if (resp == NULL) return -1;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
    dbus_message_iter_recurse(&args, &subargs);
	const char *path = NULL;
	dbus_message_iter_get_basic(&subargs, &path);

	fill_ap(ctx->conn, path, ap);

	dbus_message_unref(resp);

	return 0;
}

int pak_wifi_is_enabled(struct PakWiFi *ctx) {
	DBusConnection *conn = get_dbus_system();
	dbus_bool_t v;
	get_networkmanager_service_basic_property(conn, "WirelessEnabled", &v);
	return v;
}
