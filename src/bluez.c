// dbus bluez impl
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <dbus/dbus.h>
#include <errno.h>
#include "dbus.h"
#include "bluetooth.h"

struct PakBt {
	int foo;
};

struct PakBtSocket {
	int fd;
};

struct PakBtDevicePriv {
	int foo;
};

struct PakBtAdapterPriv {
	char path[1024];
};

struct PakBt *pak_bt_get_context(void) {
	struct PakBt *ctx = malloc(sizeof(struct PakBt));
	return ctx;
}

static inline void *alloc_priv(unsigned int sizeofstruct, const char *path) {
	unsigned int path_len = strlen(path);
	char *buf = calloc(1, sizeofstruct + path_len + 1);
	memcpy(buf + sizeofstruct, path, path_len + 1);
	return buf;
}

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

int pak_bt_is_enabled(struct PakBt *ctx) {
	DBusConnection *conn = get_dbus_system();
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

// https://github.com/bluez/bluez-sandbox/blob/209e2568c6970d174c45460d1e16c3e7e8571a5f/doc/adapter-api.txt
static int fill_adapter_from_dict(struct DBusMessageIter *dict_iter, struct PakBtAdapter *adapter, const char *path) {
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
		} else if (!strcmp(name, "Name")) {
			const char *v;
			dbus_message_iter_get_basic(&dict_variant, &v);
			strlcpy(adapter->name, v, sizeof(adapter->name));
		} else if (!strcmp(name, "Address")) {
			const char *v;
			dbus_message_iter_get_basic(&dict_variant, &v);
			strlcpy(adapter->address, v, sizeof(adapter->address));
		}

		adapter->priv = (struct PakBtAdapterPriv *)alloc_priv(sizeof(struct PakBtAdapterPriv), path);

		dbus_message_iter_next(&arr_iter);
	}
	return -1;
}

int pak_bt_get_n_adapters(struct PakBt *ctx) {
	DBusConnection *conn = get_dbus_system();

	DBusMessage *resp = send_message_noargs(conn, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
	if (resp == NULL) return -1;

	DBusMessageIter iter;
	dbus_message_iter_init(resp, &iter);

	int len = dbus_message_iter_get_element_count(&iter);
	DBusMessageIter iter_arr;
	dbus_message_iter_recurse(&iter, &iter_arr);

	int n_valid_adapters = 0;
	for (int i = 0; i < len; i++) {
		DBusMessageIter dict;
		dbus_message_iter_recurse(&iter_arr, &dict);

		dbus_message_iter_next(&dict);
		DBusMessageIter adapter_dict;
		if (!find_dict(&dict, "org.bluez.Adapter1", &adapter_dict)) {
			n_valid_adapters++;
		}

		dbus_message_iter_next(&iter_arr);
	}

	dbus_message_unref(resp);

	return n_valid_adapters;
}

int pak_bt_get_adapter(struct PakBt *ctx, struct PakBtAdapter *adapter, int index) {
	DBusConnection *conn = get_dbus_system();

	DBusMessage *resp = send_message_noargs(conn, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
	if (resp == NULL) return -1;

	DBusMessageIter iter;
	dbus_message_iter_init(resp, &iter);
	int len = dbus_message_iter_get_element_count(&iter);

	DBusMessageIter iter_arr;
	dbus_message_iter_recurse(&iter, &iter_arr);

	int n_valid_adapters = 0;
	for (int i = 0; i < len; i++) {
		DBusMessageIter dict;
		dbus_message_iter_recurse(&iter_arr, &dict);
		const char *path = NULL;
		dbus_message_iter_get_basic(&dict, &path);
		if (path == NULL) abort();
		dbus_message_iter_next(&dict);
		DBusMessageIter adapter_dict;
		if (!find_dict(&dict, "org.bluez.Adapter1", &adapter_dict) && n_valid_adapters == index) {
			fill_adapter_from_dict(&adapter_dict, adapter, path);
			n_valid_adapters++;
		}

		dbus_message_iter_next(&iter_arr);
	}

	dbus_message_unref(resp);
	return 0;
}

int pak_bt_unref_adapter(struct PakBt *ctx, struct PakBtAdapter *adapter) {
	return 0;
}

// https://github.com/bluez/bluez-sandbox/blob/209e2568c6970d174c45460d1e16c3e7e8571a5f/doc/device-api.txt
static int fill_adv_from_dict(struct DBusMessageIter *dict_iter, struct PakBtAdvertisement *adv) {
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

		if (!strcmp(name, "Connected")) {
			dbus_bool_t v;
			dbus_message_iter_get_basic(&dict_variant, &v);
		} else if (!strcmp(name, "Name")) {
			const char *v;
			dbus_message_iter_get_basic(&dict_variant, &v);
			strlcpy(adv->name, v, sizeof(adv->name));
		} else if (!strcmp(name, "Address")) {
			const char *v;
			dbus_message_iter_get_basic(&dict_variant, &v);
			strlcpy(adv->mac_address, v, sizeof(adv->mac_address));
		}

		dbus_message_iter_next(&arr_iter);
	}
	return -1;
}

int pak_bt_get_advertisements(struct PakBt *ctx, struct PakBtAdapter *adapter, struct PakBtAdvertisementList **list_arg) {
	DBusConnection *conn = get_dbus_system();

	DBusMessage *resp = send_message_noargs(conn, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
	if (resp == NULL) return -1;

	DBusMessageIter iter;
	dbus_message_iter_init(resp, &iter);

	int len = dbus_message_iter_get_element_count(&iter);
	struct PakBtAdvertisementList *list = malloc(sizeof(struct PakBtAdvertisementList) + (sizeof(struct PakBtAdvertisement) * len));
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
		if (!find_dict(&dict, "org.bluez.Device1", &adapter_dict)) {
			//list->list[list->length].priv = path; // todo: strdup?
			fill_adv_from_dict(&adapter_dict, &list->list[list->length]);
			list->length++;
		}

		dbus_message_iter_next(&iter_arr);
	}

	dbus_message_unref(resp);

	(*list_arg) = list;
	return 0;
}

// https://man.freebsd.org/cgi/man.cgi?query=bluetooth&sektion=4&manpath=OpenBSD+5.1
int pak_bt_connect_to_service_channel(struct PakBt *ctx, struct PakBtDevice *dev, uint8_t uuid128[16], struct PakBtSocket **conn) {
	bdaddr_t target;
	str2ba(dev->mac_address, &target);
	const bdaddr_t bdaddr_any = {{0, 0, 0, 0, 0, 0}};
	sdp_session_t *session = sdp_connect(&bdaddr_any, &target, SDP_RETRY_IF_BUSY);
	if (session == NULL) {
		return -1;
	}

	uuid_t svc_uuid;
	uint32_t range = 0x0000ffff;
	sdp_uuid128_create(&svc_uuid, uuid128);
	sdp_list_t *search_list = sdp_list_append(0, &svc_uuid);
	sdp_list_t *attrid_list = sdp_list_append(0, &range);

	sdp_list_t *seq;
	int rc = sdp_service_search_attr_req(session, search_list, SDP_ATTR_REQ_RANGE, attrid_list, &seq);
	sdp_list_free(search_list, 0);
	sdp_list_free(attrid_list, 0);
	if (rc) {
		fprintf(stderr, "sdp_service_search_attr_req: %d\n", rc);
		sdp_close(session);
		return -1;
	}

	uint8_t channel;
	for (; seq; seq = seq->next) {
		sdp_record_t *rec = (sdp_record_t *)seq->data;
		sdp_list_t *proto_list = NULL;
		if (sdp_get_access_protos(rec, &proto_list) == 0) {
			channel = sdp_get_proto_port(proto_list, RFCOMM_UUID);
			break;
		}
	}
	if (seq == NULL) {
		fprintf(stderr, "failed to find rfcomm port channel\n");
		return -1;
	}

	int fd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	if (fd < 0) return -1;

	(*conn) = malloc(sizeof(struct PakBtSocket));
	(*conn)->fd = fd;

	uint32_t linkmode = RFCOMM_LM_AUTH | RFCOMM_LM_ENCRYPT;
	setsockopt(fd, SOL_RFCOMM, RFCOMM_LM, &linkmode, sizeof(uint32_t));

	struct sockaddr_rc addr = {0};
	addr.rc_family = AF_BLUETOOTH;
	addr.rc_channel = channel;
	str2ba(dev->mac_address, &addr.rc_bdaddr);

	if (connect(fd, (const struct sockaddr *)&addr, sizeof(addr))) {
		fprintf(stderr, "connect: %d\n", errno);
		close(fd);
		return -1;
	}

	return 0;
}

int pak_bt_write(struct PakBtSocket *conn, const void *data, unsigned int length) {
	return write(conn->fd, data, length);
}

int pak_bt_read(struct PakBtSocket *conn, void *data, unsigned int length) {
	return read(conn->fd, data, length);
}

int pak_bt_close_socket(struct PakBtSocket *conn) {
	close(conn->fd);
	free(conn);
	return 0;
}
