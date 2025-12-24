// dbus NetworkManager impl
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <dbus/dbus.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include "dbus.h"
#include "wifi.h"

struct PakWiFi {
	DBusConnection *conn;
	DBusMessage *adapter_list;
};

struct PakWiFiAdapterPriv {
	char path[1024];
};

struct PakWiFiApPriv {
	char path[1024];
};

struct PakWiFi *pak_wifi_get_context(void) {
	struct PakWiFi *ctx = malloc(sizeof(struct PakWiFi));
	ctx->conn = get_dbus_system();
	ctx->adapter_list = NULL;
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

static int is_usable_adapter(DBusConnection *conn, const char *path) {
	uint64_t dev_type = 0;
	get_networkmanager_basic_property(conn, path, "org.freedesktop.NetworkManager.Device", "DeviceType", &dev_type);

	// Network cards can be detached from NetworkManager like so:
	// nmcli dev set <dev> managed no
	dbus_bool_t is_managed = 0;
	get_networkmanager_basic_property(conn, path, "org.freedesktop.NetworkManager.Device", "Managed", &is_managed);

	return (dev_type == 2 && is_managed); // 2 = NM_DEVICE_TYPE_WIFI
}

int pak_wifi_get_n_adapters(struct PakWiFi *ctx) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager", "org.freedesktop.DBus.Properties", "Get");

	const char *iface = "org.freedesktop.NetworkManager";
	const char *prop = "Devices";
	dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_INVALID);
	dbus_message_append_args(call, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

	DBusMessage *resp = send_reply_and_block(ctx->conn, call);
	if (resp == NULL) return -1;

	if (ctx->adapter_list != NULL) dbus_message_unref(ctx->adapter_list);
	ctx->adapter_list = resp;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
	dbus_message_iter_recurse(&args, &subargs);
	if (dbus_message_iter_get_arg_type(&subargs) != DBUS_TYPE_ARRAY) return -1;
	DBusMessageIter dict;
	dbus_message_iter_recurse(&subargs, &dict);
	int count = 0;
	while (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_INVALID) {
		const char *path = NULL;
		dbus_message_iter_get_basic(&dict, &path);
		if (path == NULL) abort();
		if (is_usable_adapter(ctx->conn, path)) count++;
		dbus_message_iter_next(&dict);
	}
	return count;
}

int pak_wifi_get_adapter(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, int index) {
	if (ctx->adapter_list == NULL) {
		if (pak_wifi_get_n_adapters(ctx) < 0) return -1;
	}

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(ctx->adapter_list, &args)) return -1;
	dbus_message_iter_recurse(&args, &subargs);

	if (dbus_message_iter_get_arg_type(&subargs) != DBUS_TYPE_ARRAY) return -1;

	DBusMessageIter dict;
	dbus_message_iter_recurse(&subargs, &dict);
	int current_type;
	int count = 0;
	while ((current_type = dbus_message_iter_get_arg_type(&dict)) != DBUS_TYPE_INVALID) {
		const char *path = NULL;
		dbus_message_iter_get_basic(&dict, &path);
		if (path == NULL) abort();
		if (is_usable_adapter(ctx->conn, path)) {
			// Return first available as default for now
			if (index == count || index == -1) {
				adapter->priv = (struct PakWiFiAdapterPriv *)strdup(path);
				const char *dev_iface = NULL;
				get_networkmanager_basic_property(ctx->conn, path, "org.freedesktop.NetworkManager.Device", "Interface", &dev_iface);
				strlcpy(adapter->name, dev_iface, sizeof(adapter->name));
				// TODO: Get ip address
				// https://people.freedesktop.org/~lkundrak/nm-docs/gdbus-org.freedesktop.NetworkManager.IP4Config.html
//				const char *ip4config = NULL;
//				get_networkmanager_basic_property(ctx->conn, path, "org.freedesktop.NetworkManager.Device", "Ip4Config", &ip4config);
//				printf("%s\n", ip4config);
				return 0;
			} else {
				count++;
			}
		}
		dbus_message_iter_next(&dict);
	}

	return -1;
}

int pak_wifi_bind_socket_to_adapter(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, int fd) {
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, adapter->name, sizeof(ifr.ifr_name));
	return setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr));
}

int pak_wifi_unref_adapter(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter_arg) {
	free(adapter_arg->priv->path);
	return 0;
}

static int fill_ap(DBusConnection *conn, const char *path, struct PakWiFiAp *ap) {
	char *s = NULL;
	get_networkmanager_u8array_property(conn, path, "org.freedesktop.NetworkManager.AccessPoint", "Ssid", &s);
	strlcpy(ap->ssid, s, sizeof(ap->ssid));
	free(s);
	get_networkmanager_basic_property(conn, path, "org.freedesktop.NetworkManager.AccessPoint", "HwAddress", &s);
	strlcpy(ap->bssid, s, sizeof(ap->bssid));
	uint64_t freq = 0;
	get_networkmanager_basic_property(conn, path, "org.freedesktop.NetworkManager.AccessPoint", "Frequency", &freq);
	if (freq & 0x5000) ap->band = PAK_WIFI_5GHZ;
	if (freq & 0x2000) ap->band = PAK_WIFI_2GHZ;

	ap->priv = (struct PakWiFiApPriv *)strdup(path);

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

int pak_wifi_get_connected_ap(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", adapter->priv->path, "org.freedesktop.DBus.Properties", "Get");

	const char *iface = "org.freedesktop.NetworkManager.Device.Wireless";
	const char *prop = "ActiveAccessPoint";
	dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

	DBusMessage *resp = send_reply_and_block(ctx->conn, call);
	if (resp == NULL) return -1;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
	dbus_message_iter_recurse(&args, &subargs);
	const char *ap_path = NULL;
	dbus_message_iter_get_basic(&subargs, &ap_path);

	fill_ap(ctx->conn, ap_path, ap);

	dbus_message_unref(resp);

	return 0;
}

int pak_wifi_is_enabled(struct PakWiFi *ctx) {
	DBusConnection *conn = get_dbus_system();
	dbus_bool_t v;
	get_networkmanager_service_basic_property(conn, "WirelessEnabled", &v);
	return v;
}

int pak_wifi_connect_to_ap(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap) {
	const char *existing_conn = "/";
	const char *device = adapter->priv->path;
	const char *ap_path = ap->priv->path;

	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager", "org.freedesktop.NetworkManager", "ActivateConnection");
	dbus_message_append_args(call, DBUS_TYPE_OBJECT_PATH, &existing_conn, DBUS_TYPE_OBJECT_PATH, &device, DBUS_TYPE_OBJECT_PATH, &ap_path, DBUS_TYPE_INVALID);

	DBusMessage *resp = send_reply_and_block(ctx->conn, call);
	if (resp == NULL) return -1;

	dbus_message_unref(resp);

	return 0;
}
