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
	DBusMessage *current_ap_list;
	char path[];
};

struct PakWiFiApPriv {
	int x;
	char path[];
};

static DBusHandlerResult handle_messages(DBusConnection *connection, DBusMessage *message, void *user_data) {
	printf("Handle message: %s\n", dbus_message_get_member(message));
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

struct PakWiFi *pak_wifi_get_context(void) {
	struct PakWiFi *ctx = malloc(sizeof(struct PakWiFi));
	ctx->conn = get_dbus_system();
	ctx->adapter_list = NULL;

	DBusConnection *conn = get_dbus_system();

	dbus_connection_add_filter(conn, handle_messages, ctx, NULL);

	dbus_bus_add_match(conn,
		"type='signal',"
		"sender='org.freedesktop.NetworkManager',"
		"interface='org.freedesktop.NetworkManager.AccessPoint',"
		"member='PropertiesChanged'"
		, NULL);
	dbus_connection_flush(conn);

	return ctx;
}

static int get_networkmanager_basic_property(DBusConnection *conn, const char *path, const char *iface, const char *prop, void *val, DBusMessage **resp) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", path, "org.freedesktop.DBus.Properties", "Get");
	dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

	(*resp) = send_reply_and_block(conn, call);
	if (resp == NULL) return -1;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(*resp, &args)) return -1;
	dbus_message_iter_recurse(&args, &subargs);
	dbus_message_iter_get_basic(&subargs, val); // is this a use after free?

	return 0;
}

static int sleep_for_event(DBusConnection *conn) {
	while (dbus_connection_read_write_dispatch(conn, 1000) == FALSE);
	return 0;
}

static int get_networkmanager_u8array_property(DBusConnection *conn, const char *path, const char *iface, const char *prop, char *val, unsigned int max) {
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

	if (get_u8array(&subargs, val, max)) return -1;

	dbus_message_unref(resp);

	return 0;
}

static int get_networkmanager_service_basic_property(DBusConnection *conn, const char *prop, void *val, DBusMessage **resp) {
	get_networkmanager_basic_property(conn, "/org/freedesktop/NetworkManager", "org.freedesktop.NetworkManager", prop, val, resp);
	return 0;
}

static int is_usable_adapter(DBusConnection *conn, const char *path) {
	DBusMessage *resp;

	uint64_t dev_type = 0;
	get_networkmanager_basic_property(conn, path, "org.freedesktop.NetworkManager.Device", "DeviceType", &dev_type, &resp);
	dbus_message_unref(resp);

	// Network cards can be detached from NetworkManager like so:
	// nmcli dev set <dev> managed no
	dbus_bool_t is_managed = 0;
	get_networkmanager_basic_property(conn, path, "org.freedesktop.NetworkManager.Device", "Managed", &is_managed, &resp);
	dbus_message_unref(resp);

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
		DBusMessage *tempresp;
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
				adapter->priv = (struct PakWiFiAdapterPriv *)alloc_priv(sizeof(struct PakWiFiAdapterPriv), path);
				const char *dev_iface = NULL;

				DBusMessage *tempresp;
				get_networkmanager_basic_property(ctx->conn, path, "org.freedesktop.NetworkManager.Device", "Interface", &dev_iface, &tempresp);
				strlcpy(adapter->name, dev_iface, sizeof(adapter->name));
				dbus_message_unref(tempresp);
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
	free(adapter_arg->priv);
	return 0;
}

static int fill_ap(DBusConnection *conn, const char *path, struct PakWiFiAp *ap) {
	DBusMessage *tempresp;
	const char *v;
	char ssid[64];
	get_networkmanager_u8array_property(conn, path, "org.freedesktop.NetworkManager.AccessPoint", "Ssid", ssid, sizeof(ssid));
	strlcpy(ap->ssid, ssid, sizeof(ap->ssid));
	get_networkmanager_basic_property(conn, path, "org.freedesktop.NetworkManager.AccessPoint", "HwAddress", &v, &tempresp);
	strlcpy(ap->bssid, v, sizeof(ap->bssid));
	dbus_message_unref(tempresp);
	uint64_t freq = 0;
	get_networkmanager_basic_property(conn, path, "org.freedesktop.NetworkManager.AccessPoint", "Frequency", &freq, &tempresp);
	if (freq & 0x5000) ap->band = PAK_WIFI_5GHZ;
	if (freq & 0x2000) ap->band = PAK_WIFI_2GHZ;
	dbus_message_unref(tempresp);

	ap->priv = (struct PakWiFiApPriv *)alloc_priv(sizeof(struct PakWiFiApPriv), path);

	return 0;
}

int pak_wifi_get_n_aps(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", adapter->priv->path, "org.freedesktop.DBus.Properties", "Get");

	const char *iface = "org.freedesktop.NetworkManager.Device.Wireless";
	const char *prop = "AccessPoints";
	dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

	DBusMessage *resp = send_reply_and_block(ctx->conn, call);
	if (resp == NULL) return -1;

	if (adapter->priv->current_ap_list != NULL) dbus_message_unref(adapter->priv->current_ap_list);
	adapter->priv->current_ap_list = resp;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
	dbus_message_iter_recurse(&args, &subargs);

	int len = dbus_message_iter_get_element_count(&subargs);
	return len;
}

int pak_wifi_get_ap(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap, int index) {
	if (adapter->priv->current_ap_list == NULL) {
		if (pak_wifi_get_n_aps(ctx, adapter) < 0) return -1;
	}

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(adapter->priv->current_ap_list, &args)) return -1;
	dbus_message_iter_recurse(&args, &subargs);

	if (dbus_message_iter_get_arg_type(&subargs) != DBUS_TYPE_ARRAY) return -1;

	DBusMessageIter dict;
	dbus_message_iter_recurse(&subargs, &dict);
	int len = dbus_message_iter_get_element_count(&subargs);

	int current_type;
	for (int i = 0; i < len; i++) {
		if (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_OBJECT_PATH) return -1;
		const char *path = NULL;
		dbus_message_iter_get_basic(&dict, &path);

		if (i == index) {
			fill_ap(ctx->conn, path, ap);
			return 0;
		}

		dbus_message_iter_next(&dict);
	}

	return -1;
}

int pak_wifi_unref_ap(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap) {
	free(ap->priv);
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
	DBusMessage *resp;
	DBusConnection *conn = get_dbus_system();
	dbus_bool_t v;
	get_networkmanager_service_basic_property(conn, "WirelessEnabled", &v, &resp);
	dbus_message_unref(resp);
	return v;
}

int pak_wifi_request_scan(struct PakWiFi *ctx) {
	DBusConnection *conn = get_dbus_system();
	return 0;
}

static int add_string_key(DBusMessageIter *con_val, const char *key, const char *val) {
	DBusMessageIter item;
	DBusMessageIter var;
	dbus_message_iter_open_container(con_val, DBUS_TYPE_DICT_ENTRY, NULL, &item);
	dbus_message_iter_append_basic(&item, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&item, DBUS_TYPE_VARIANT, "s", &var);
	dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &val);
	dbus_message_iter_close_container(&item, &var);
	dbus_message_iter_close_container(con_val, &item);
	return 0;
}

static int add_bytearray_key(DBusMessageIter *con_val, const char *key, const char *val) {
	DBusMessageIter item;
	DBusMessageIter var;
	dbus_message_iter_open_container(con_val, DBUS_TYPE_DICT_ENTRY, NULL, &item);
	dbus_message_iter_append_basic(&item, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&item, DBUS_TYPE_VARIANT, "ay", &var);
	DBusMessageIter byte_array;
	dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, "y", &byte_array);
	for (int i = 0; val[i] != '\0'; i++) {
		dbus_message_iter_append_basic(&byte_array, DBUS_TYPE_BYTE, &val[i]);
	}
	dbus_message_iter_close_container(&var, &byte_array);
	dbus_message_iter_close_container(&item, &var);
	dbus_message_iter_close_container(con_val, &item);
	return 0;
}

static int add_bool_key(DBusMessageIter *con_val, const char *key, dbus_bool_t val) {
	DBusMessageIter item;
	DBusMessageIter var;
	dbus_message_iter_open_container(con_val, DBUS_TYPE_DICT_ENTRY, NULL, &item);
	dbus_message_iter_append_basic(&item, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&item, DBUS_TYPE_VARIANT, "b", &var);
	dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &val);
	dbus_message_iter_close_container(&item, &var);
	dbus_message_iter_close_container(con_val, &item);
	return 0;
}

static void map_begin_setting(DBusMessageIter *map, DBusMessageIter *entry, DBusMessageIter *val, const char *key) {
	dbus_message_iter_open_container(map, DBUS_TYPE_DICT_ENTRY, NULL, entry);
	dbus_message_iter_append_basic(entry, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(entry, DBUS_TYPE_ARRAY, "{sv}", val);	
}

// https://people.freedesktop.org/~lkundrak/nm-dbus-api/nm-settings.html
static int append_pack_connection_info(DBusConnection *conn, DBusMessageIter *iter, struct PakWiFiAp *ap, const char *password) {
	DBusMessageIter dict;
	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "{sa{sv}}", &dict);

	DBusMessageIter con_entry;
	DBusMessageIter con_val;

	map_begin_setting(&dict, &con_entry, &con_val, "connection");
	add_string_key(&con_val, "id", "IoT WiFi Device Connection");
	add_string_key(&con_val, "type", "802-11-wireless");
	add_string_key(&con_val, "zone", "PakDevice");
	//add_string_key(&con_val, "interface-name", "wlp19s0");
	add_bool_key(&con_val, "autoconnect", FALSE);
	dbus_message_iter_close_container(&con_entry, &con_val);
	dbus_message_iter_close_container(&dict, &con_entry);

	map_begin_setting(&dict, &con_entry, &con_val, "802-11-wireless");
	add_bytearray_key(&con_val, "ssid", ap->ssid);
	dbus_message_iter_close_container(&con_entry, &con_val);
	dbus_message_iter_close_container(&dict, &con_entry);

	map_begin_setting(&dict, &con_entry, &con_val, "802-11-wireless-security");
	add_string_key(&con_val, "key-mgmt", "wpa-psk");
	if (password != NULL) add_string_key(&con_val, "psk", password);
	dbus_message_iter_close_container(&con_entry, &con_val);
	dbus_message_iter_close_container(&dict, &con_entry);

	dbus_message_iter_close_container(iter, &dict);
	return 0;
}

static int is_connection_matching(DBusConnection *conn, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap, const char *path) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", path, "org.freedesktop.NetworkManager.Settings.Connection", "GetSettings");

	DBusMessage *resp = send_reply_and_block(conn, call);
	if (resp == NULL) return -1;

	DBusMessageIter args;
	if (!dbus_message_iter_init(resp, &args)) return -1;

	int matched_ssid = 0;

	DBusMessageIter dict;
	dbus_message_iter_recurse(&args, &dict);
	while (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_INVALID) {
		DBusMessageIter subargs;
		dbus_message_iter_recurse(&dict, &subargs);
		const char *s = NULL;
		dbus_message_iter_get_basic(&subargs, &s);
		dbus_message_iter_next(&subargs);
		if (!strcmp(s, "802-11-wireless")) {
			DBusMessageIter subsubargs;
			dbus_message_iter_recurse(&subargs, &subsubargs);
			while (dbus_message_iter_get_arg_type(&subsubargs) != DBUS_TYPE_INVALID) {
				DBusMessageIter subsubsubargs;
				dbus_message_iter_recurse(&subsubargs, &subsubsubargs);
				dbus_message_iter_get_basic(&subsubsubargs, &s);
				dbus_message_iter_next(&subsubsubargs);
				if (!strcmp(s, "ssid")) {
					DBusMessageIter subsubsubsubargs;
					dbus_message_iter_recurse(&subsubsubargs, &subsubsubsubargs);
					char ssid[64];
					get_u8array(&subsubsubsubargs, ssid, sizeof(ssid));
					matched_ssid = !strcmp(ssid, ap->ssid);
				}
				dbus_message_iter_next(&subsubargs);
			}
		}
		dbus_message_iter_next(&dict);
	}

	dbus_message_unref(resp);

	if (matched_ssid) {
		return 1;
	}

	//printf("%c\n", dbus_message_iter_get_arg_type(&subsubsubsubargs));

	return 0;
}

static int get_existing_connection(DBusConnection *conn, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap, const char **path_arg) {

	const char *iface = "org.freedesktop.NetworkManager.Device";
	const char *prop = "AvailableConnections";

	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", adapter->priv->path, "org.freedesktop.DBus.Properties", "Get");
	dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);

	DBusMessage *resp = send_reply_and_block(conn, call);
	if (resp == NULL) return -1;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
	dbus_message_iter_recurse(&args, &subargs);

	if (dbus_message_iter_get_arg_type(&subargs) != DBUS_TYPE_ARRAY) return -1;

	DBusMessageIter dict;
	dbus_message_iter_recurse(&subargs, &dict);
	int current_type;
	int i = 0;
	while ((current_type = dbus_message_iter_get_arg_type(&dict)) != DBUS_TYPE_INVALID) {
		const char *path = NULL;
		dbus_message_iter_get_basic(&dict, &path);
		if (is_connection_matching(conn, adapter, ap, path)) {
			printf("Found existing connection path: %s\n", path);
			(*path_arg) = path;
			dbus_message_unref(resp);
			return 1;
		}
		dbus_message_iter_next(&dict);
	}

	dbus_message_unref(resp);

	return 0;
}

int pak_wifi_connect_to_ap(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap, const char *password) {
	DBusError error;
	dbus_error_init(&error);

	const char *device = adapter->priv->path;
	const char *ap_path = ap->priv->path;

	const char *conn_path = NULL;
	if (get_existing_connection(ctx->conn, adapter, ap, &conn_path)) {

		// TODO: Update connection setting with new password (if changed)

		DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager", "org.freedesktop.NetworkManager", "ActivateConnection");
		DBusMessageIter iter;
		dbus_message_iter_init_append(call, &iter);
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &conn_path); // arg0: existing connection path
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &device); // arg1: device
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &ap_path); // arg2: ap path

		DBusMessage *resp = send_reply_and_block(ctx->conn, call);
		if (resp == NULL) return -1;

		dbus_message_unref(resp);
	} else {
		DBusMessage *call = dbus_message_new_method_call("org.freedesktop.NetworkManager", "/org/freedesktop/NetworkManager", "org.freedesktop.NetworkManager", "AddAndActivateConnection");
		DBusMessageIter iter;
		dbus_message_iter_init_append(call, &iter);
		append_pack_connection_info(ctx->conn, &iter, ap, password); // arg0: connection settings
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &device); // arg1: device
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &ap_path); // arg2: ap path

		DBusMessage *resp = send_reply_and_block(ctx->conn, call);
		if (resp == NULL) return -1;

		dbus_message_unref(resp);
	}

	sleep_for_event(ctx->conn);
	sleep_for_event(ctx->conn);
	sleep_for_event(ctx->conn);
	sleep_for_event(ctx->conn);
	sleep_for_event(ctx->conn);
	sleep_for_event(ctx->conn);

	return 0;
}
