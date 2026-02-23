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
	DBusConnection *conn;
	pak_bt_listen_gatt *listen_gatt;
	pak_bt_listen_adv *listen_adv;
};

struct PakBtSocket {
	int fd;
};

struct PakBtDevicePriv {
	int x;
	char path[];
};

struct PakBtAdapterPriv {
	int x;
	char path[];
};

struct PakGattServicePriv {
	int x;
	char path[];
};

struct PakGattCharacteristicPriv {
	int x;
	char path[];
};

static int pak_str_to_uuid128(const char *str, uint8_t copy[16]) {
	int32_t v[16];
	if (sscanf(str, "%2x%2x%2x%2x-%2x%2x-%2x%2x-%2x%2x-%2x%2x%2x%2x%2x%2x",
			&v[0], &v[1], &v[2], &v[3],
			&v[4], &v[5],
			&v[6], &v[7],
			&v[8], &v[9],
			&v[10], &v[11], &v[12], &v[13], &v[14], &v[15]) != 16)
		return -1;
	for (int i = 0; i < 16; i++) {
		copy[i] = v[i];
	}
	return 0;
}

static void pak_uuid128_to_str(const uint8_t in[16], char str[37]) {
	sprintf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		in[0], in[1], in[2], in[3],
		in[4], in[5],
		in[6], in[7],
		in[8], in[9],
		in[10], in[11], in[12], in[13], in[14], in[15]);
}

static DBusHandlerResult handle_messages(DBusConnection *conn, DBusMessage *message, void *user_data) {
	if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (dbus_message_is_signal(message, "org.freedesktop.DBus.Properties", "PropertiesChanged")) {
		printf("Property changed\n");

		DBusMessageIter iter, iter2, iter3;
		dbus_message_iter_init(message, &iter);
		const char *iface_name;
		dbus_message_iter_get_basic(&iter, &iface_name);
		printf("iface: %s\n", iface_name);
		dbus_message_iter_next(&iter); // skip iface
		dbus_message_iter_recurse(&iter, &iter2);

		dbus_message_iter_recurse(&iter2, &iter3);

		const char *signal_name;
		dbus_message_iter_get_basic(&iter3, &signal_name);
		printf("changed %s\n", signal_name);

		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	} else if (dbus_message_is_signal(message, "org.bluez.Adapter", "DeviceFound")) {
		printf("Device found\n");
	}

	if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_SIGNAL) {
		const char *iface;
		const char *member;
		const char *path;
		iface = dbus_message_get_interface(message);
		member = dbus_message_get_member(message);
		path = dbus_message_get_path(message);

		printf("signal iface=%s member=%s path=%s\n",
		       iface ? iface : "(null)",
		       member ? member : "(null)",
		       path ? path : "(null)");
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

struct PakBt *pak_bt_get_context(void) {
	struct PakBt *ctx = malloc(sizeof(struct PakBt));

	ctx->conn = get_dbus_system();

	dbus_connection_add_filter(ctx->conn, handle_messages, NULL, NULL);

	dbus_bus_add_match(ctx->conn,
		"type='signal',"
		"sender='org.bluez',"
		"interface='org.freedesktop.DBus.Properties',"
		"member='PropertiesChanged'"
		, NULL);
	dbus_connection_flush(ctx->conn);

	return ctx;
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

static int get_bluez_string_property(DBusConnection *conn, const char *path, const char *iface, const char *prop, const char **v, DBusMessage **resp) {
	int rc = get_dbus_property(conn, "org.bluez", path, iface, prop, resp);

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(*resp, &args)) return -1;
    dbus_message_iter_recurse(&args, &subargs);
    dbus_message_iter_get_basic(&subargs, v);

	return 0;
}

int pak_bt_is_enabled(struct PakBt *ctx) {
	DBusConnection *conn = get_dbus_system();
	dbus_bool_t v;
	get_bluez_bool_property(conn, "/org/bluez/hci0", "org.bluez.Adapter1", "Powered", &v);
	return v;
}

// Find dict within a dict
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
	free(adapter->priv);
	return 0;
}

static void parse_manufacturer_data(struct DBusMessageIter *dict, struct PakBtDevice *dev) {
	// Example: "ManufacturerData" a{qv} 1 1520 ay 4 170 0 0 0
	int of = 0;
	struct DBusMessageIter dict2;
	dbus_message_iter_recurse(dict, &dict2);

	struct DBusMessageIter dict3;
	dbus_message_iter_recurse(&dict2, &dict3);

	while (dbus_message_iter_get_arg_type(&dict3) != DBUS_TYPE_INVALID) {
		struct DBusMessageIter dict4;
		dbus_message_iter_recurse(&dict3, &dict4);

		uint16_t qword;
		dbus_message_iter_get_basic(&dict4, &qword);

		dev->mfg_data[of++] = qword & 0xff;
		dev->mfg_data[of++] = (qword >> 8) & 0xff;

		dbus_message_iter_next(&dict4);

		struct DBusMessageIter dict5;
		dbus_message_iter_recurse(&dict4, &dict5);

		struct DBusMessageIter dict6;
		dbus_message_iter_recurse(&dict5, &dict6);

		while (dbus_message_iter_get_arg_type(&dict6) != DBUS_TYPE_INVALID) {
			uint8_t byte;
			dbus_message_iter_get_basic(&dict6, &byte);
			dev->mfg_data[of++] = byte;
			dbus_message_iter_next(&dict6);
		}

		dbus_message_iter_next(&dict3);
	}
}

// https://git.kernel.org/pub/scm/bluetooth/bluez.git/tree/doc/org.bluez.Device.rst
static int fill_from_device1(struct DBusMessageIter *dict_iter, struct PakBtDevice *dev) {
	int len = dbus_message_iter_get_element_count(dict_iter);
	DBusMessageIter arr_iter;
	dbus_message_iter_recurse(dict_iter, &arr_iter);
	dev->is_classic = 1;
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
			dev->is_connected = (int)v;
		} else if (!strcmp(name, "Name")) {
			const char *v;
			dbus_message_iter_get_basic(&dict_variant, &v);
			strlcpy(dev->name, v, sizeof(dev->name));
		} else if (!strcmp(name, "Address")) {
			const char *v;
			dbus_message_iter_get_basic(&dict_variant, &v);
			strlcpy(dev->mac_address, v, sizeof(dev->mac_address));
		} else if (!strcmp(name, "ManufacturerData")) {
			parse_manufacturer_data(&dict, dev);
			dev->is_classic = 0; // not ideal way to tell classic/ble
		} else if (!strcmp(name, "UUIDs")) {
			struct DBusMessageIter dict2;
			dbus_message_iter_recurse(&dict, &dict2);

			dev->uuids.length = (unsigned int)dbus_message_iter_get_element_count(&dict2);
			dev->uuids.uuids = malloc(sizeof(struct PakUuidList) + UUID_STR_LENGTH * dev->uuids.length);

			struct DBusMessageIter dict3;
			dbus_message_iter_recurse(&dict2, &dict3);

			int i = 0;
			while (dbus_message_iter_get_arg_type(&dict3) != DBUS_TYPE_INVALID) {
				const char *uuid = NULL;
				dbus_message_iter_get_basic(&dict3, &uuid);
				strlcpy(dev->uuids.uuids[i], uuid, UUID_STR_LENGTH);
				dbus_message_iter_next(&dict3);
				i++;
			}
		} else if (!strcmp(name, "Class")) {
			uint32_t v;
			dbus_message_iter_get_basic(&dict_variant, &v);
			dev->btclass = v;
		}

		dbus_message_iter_next(&arr_iter);
	}
	return 0;
}

#define FILTER_IS_CONNECTED (1 << 1)
#define FILTER_IS_SAVED (1 << 2)

static int pak_bt_get_object(struct PakBt *ctx, struct PakBtAdapter *adapter, struct PakBtDevice *dev, int index, int filter) {
	DBusConnection *conn = get_dbus_system();

	DBusMessage *resp = send_message_noargs(conn, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
	if (resp == NULL) return -1;

	DBusMessageIter iter;
	dbus_message_iter_init(resp, &iter);

	int len = dbus_message_iter_get_element_count(&iter);

	DBusMessageIter iter_arr;
	dbus_message_iter_recurse(&iter, &iter_arr);

	int found = 0;
	for (int i = 0; i < len; i++) {
		DBusMessageIter dict;
		dbus_message_iter_recurse(&iter_arr, &dict);

		const char *path = NULL;
		dbus_message_iter_get_basic(&dict, &path);
		if (path == NULL) abort();

		dbus_message_iter_next(&dict);
		DBusMessageIter adapter_dict;
		if (!find_dict(&dict, "org.bluez.Device1", &adapter_dict)) {
			dbus_bool_t is_connected = 0;
			DBusMessageIter val_dict;
			if (find_dict(&adapter_dict, "Connected", &val_dict)) return -1;
			DBusMessageIter val_dict2;
			dbus_message_iter_recurse(&val_dict, &val_dict2);
			dbus_message_iter_get_basic(&val_dict2, &is_connected);

			if (filter & (FILTER_IS_CONNECTED | FILTER_IS_SAVED)) {
				if (is_connected && (filter & FILTER_IS_CONNECTED)) {
					if (found == index) {
						fill_from_device1(&adapter_dict, dev);
						dev->priv = (struct PakBtDevicePriv *)alloc_priv(sizeof(struct PakBtDevicePriv), path);
						dbus_message_unref(resp);
						return 0;
					}
					found++;
				} else if (filter & FILTER_IS_SAVED) {
					if (found == index) {
						fill_from_device1(&adapter_dict, dev);
						dev->priv = (struct PakBtDevicePriv *)alloc_priv(sizeof(struct PakBtDevicePriv), path);
						dbus_message_unref(resp);
						return 0;
					}
					found++;
				}
			} else {
				found++;
			}
		}

		dbus_message_iter_next(&iter_arr);
	}

	dbus_message_unref(resp);

	return -1;
}

int pak_bt_get_paired_device(struct PakBt *ctx, struct PakBtAdapter *adapter, struct PakBtDevice *device, int index) {
	return pak_bt_get_object(ctx, adapter, device, index, FILTER_IS_CONNECTED);
}

int pak_bt_get_saved_device(struct PakBt *ctx, struct PakBtAdapter *adapter, struct PakBtDevice *device, int index) {
	return pak_bt_get_object(ctx, adapter, device, index, FILTER_IS_SAVED);
}

int pak_bt_unref_device(struct PakBt *ctx, struct PakBtDevice *device) {
	free(device->priv);
	free(device->uuids.uuids);
	return 0;
}

int pak_bt_get_device_battery(struct PakBt *ctx, struct PakBtDevice *device, int *percent) {
	DBusMessage *resp;
	int rc = get_dbus_property(ctx->conn, "org.bluez", device->priv->path, "org.bluez.Battery1", "Percentage", &resp);
	if (resp == NULL) return -1;

	DBusMessageIter args;
	DBusMessageIter subargs;
	if (!dbus_message_iter_init(resp, &args)) return -1;
    dbus_message_iter_recurse(&args, &subargs);

	uint8_t v;
    dbus_message_iter_get_basic(&subargs, &v);
	(*percent) = (int)v;

    dbus_message_unref(resp);

	return 0;
}

int pak_bt_device_connect(struct PakBt *ctx, struct PakBtDevice *device) {
	DBusMessage *resp = send_message_noargs(ctx->conn, "org.bluez", device->priv->path, "org.bluez.Device1", "Connect");
	if (resp == NULL) return -1;
    dbus_message_unref(resp);
	return 0;
}

#define FILTER_GATT_SERVICE (1 << 3)
#define FILTER_GATT_CHARACTERISTIC (1 << 4)
#define FILTER_GATT_DESCRIPTOR (1 << 5)

// Some parameters are allowed to be NULL for different filter flags
static int pak_bt_get_gatt_object(struct PakBt *ctx, struct PakBtDevice *device, struct PakGattService *service, struct PakGattCharacteristic *characteristic, int index, int filter) {
	DBusConnection *conn = get_dbus_system();

	DBusMessage *resp = send_message_noargs(conn, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
	if (resp == NULL) return -1;

	DBusMessageIter iter;
	dbus_message_iter_init(resp, &iter);

	int len = dbus_message_iter_get_element_count(&iter);

	DBusMessageIter iter_arr;
	dbus_message_iter_recurse(&iter, &iter_arr);

	int found = 0;
	for (int i = 0; i < len; i++) {
		DBusMessageIter dict;
		dbus_message_iter_recurse(&iter_arr, &dict);

		const char *path = NULL;
		dbus_message_iter_get_basic(&dict, &path);
		if (path == NULL) abort();

		dbus_message_iter_next(&dict);
		DBusMessageIter adapter_dict;
		if ((filter & FILTER_GATT_SERVICE) && !find_dict(&dict, "org.bluez.GattService1", &adapter_dict)) {
			if (service == NULL) pak_abort("NULL");
			if (device == NULL) pak_abort("NULL");
			const char *device_path = NULL;
			DBusMessageIter val_dict;
			if (find_dict(&adapter_dict, "Device", &val_dict)) return -1;
			DBusMessageIter val_dict2;
			dbus_message_iter_recurse(&val_dict, &val_dict2);
			dbus_message_iter_get_basic(&val_dict2, &device_path);

			if (found == index && !strcmp(device_path, device->priv->path)) {
				service->priv = (struct PakGattServicePriv *)alloc_priv(sizeof(struct PakGattServicePriv), path);

				const char *uuid;
				uint16_t handle;
				if (find_dict(&adapter_dict, "UUID", &val_dict)) return -1;
				dbus_message_iter_recurse(&val_dict, &val_dict2);
				dbus_message_iter_get_basic(&val_dict2, &uuid);
				strlcpy(service->uuid, uuid, sizeof(service->uuid));

				if (find_dict(&adapter_dict, "Handle", &val_dict)) return -1;
				dbus_message_iter_recurse(&val_dict, &val_dict2);
				dbus_message_iter_get_basic(&val_dict2, &service->handle);
				return 0;
			}
			found++;
		} else if ((filter & FILTER_GATT_CHARACTERISTIC) && !find_dict(&dict, "org.bluez.GattCharacteristic1", &adapter_dict)) {
			if (service == NULL) pak_abort("NULL");
			if (characteristic == NULL) pak_abort("NULL");
			const char *service_path = NULL;
			DBusMessageIter val_dict;
			if (find_dict(&adapter_dict, "Service", &val_dict)) return -1;
			DBusMessageIter val_dict2;
			dbus_message_iter_recurse(&val_dict, &val_dict2);
			dbus_message_iter_get_basic(&val_dict2, &service_path);

			if (found == index && !strcmp(service_path, service->priv->path)) {
				characteristic->priv = (struct PakGattCharacteristicPriv *)alloc_priv(sizeof(struct PakGattCharacteristicPriv), path);

				const char *uuid;
				uint16_t handle;
				if (find_dict(&adapter_dict, "UUID", &val_dict)) return -1;
				dbus_message_iter_recurse(&val_dict, &val_dict2);
				dbus_message_iter_get_basic(&val_dict2, &uuid);
				strlcpy(characteristic->uuid, uuid, sizeof(characteristic->uuid));

				if (find_dict(&adapter_dict, "Handle", &val_dict)) return -1;
				dbus_message_iter_recurse(&val_dict, &val_dict2);
				dbus_message_iter_get_basic(&val_dict2, &characteristic->handle);
				return 0;
			}
			found++;
		}

		dbus_message_iter_next(&iter_arr);
	}

	dbus_message_unref(resp);

	return -1;
}

int pak_bt_get_gatt_characteristic(struct PakBt *ctx, struct PakGattService *service, struct PakGattCharacteristic *characteristic, int index) {
	return pak_bt_get_gatt_object(ctx, NULL, service, characteristic, index, FILTER_GATT_CHARACTERISTIC);
}

int pak_bt_unref_gatt_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *chr) {
	free(chr->priv);
	return 0;
}

int pak_bt_get_gatt_service(struct PakBt *ctx, struct PakBtDevice *device, struct PakGattService *service, int index) {
	return pak_bt_get_gatt_object(ctx, device, service, NULL, index, FILTER_GATT_SERVICE);
}

int pak_bt_unref_gatt_service(struct PakBt *ctx, struct PakGattService *service) {
	free(service->priv);
	return 0;
}

int pak_bt_read_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *characteristic) {
	DBusMessage *resp = send_message_noargs(ctx->conn, "org.bluez", characteristic->priv->path, "org.bluez.GattCharacteristic1", "ReadValue");
	if (resp == NULL) return -1;
    dbus_message_unref(resp);
	return 0;
}

int pak_bt_write_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *characteristic, uint8_t *data, unsigned int length) {
	// TODO:
	
	//DBusMessage *resp = send_message_noargs(ctx->conn, "org.bluez", characteristic->priv->path, "org.bluez.GattCharacteristic1", "WriteValue");
	return 0;
}

static int get_service_channel(const char *mac_address, const char *uuid) {
	bdaddr_t target;
	str2ba(mac_address, &target);
	const bdaddr_t bdaddr_any = {{0, 0, 0, 0, 0, 0}};
	sdp_session_t *session = sdp_connect(&bdaddr_any, &target, SDP_RETRY_IF_BUSY);
	if (session == NULL) {
		return -1;
	}

	uint8_t uuid128[17];
	pak_str_to_uuid128(uuid, uuid128);

	uuid_t svc_uuid;
	uint32_t range = 0x0000ffff;
	sdp_uuid128_create(&svc_uuid, uuid128);
	sdp_list_t *search_list = sdp_list_append(0, &svc_uuid);
	sdp_list_t *attrid_list = sdp_list_append(0, &range);

	sdp_list_t *seq;
	int rc = sdp_service_search_attr_req(session, search_list, SDP_ATTR_REQ_RANGE, attrid_list, &seq);
	sdp_list_free(search_list, 0);
	sdp_list_free(attrid_list, 0);
	sdp_close(session);
	if (rc) {
		fprintf(stderr, "sdp_service_search_attr_req: %d\n", rc);
		return -1;
	}

	int channel = -1;
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
	return channel;
}

// https://man.freebsd.org/cgi/man.cgi?query=bluetooth&sektion=4&manpath=OpenBSD+5.1
int pak_bt_connect_to_service_channel(struct PakBt *ctx, struct PakBtDevice *dev, const char *uuid, struct PakBtSocket **conn) {
	int channel = get_service_channel(dev->mac_address, uuid);
	if (channel < 0) return -1;

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
