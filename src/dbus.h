// dbus helpers
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <dbus/dbus.h>

static inline void *alloc_priv(unsigned int sizeofstruct, const char *path) {
	unsigned int path_len = strlen(path);
	char *buf = calloc(1, sizeofstruct + path_len + 1);
	memcpy(buf + sizeofstruct, path, path_len + 1);
	return buf;
}

static DBusConnection *get_dbus_system(void) {
	DBusError error;
	dbus_error_init(&error);
	DBusConnection *connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	if (dbus_error_is_set(&error)) abort();
	return connection;
}

static DBusMessage *send_message_noargs(DBusConnection *conn, const char *dest, const char *path, const char *iface, const char *method) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call(dest, path, iface, method);
	if (call == NULL) return NULL;
	DBusMessage *resp = dbus_connection_send_with_reply_and_block(conn, call, DBUS_TIMEOUT_USE_DEFAULT, &error);
	if (resp == NULL) return NULL;
	dbus_message_unref(call);
	if (dbus_error_is_set(&error)) return NULL;
	return resp;
}

static DBusMessage *send_reply_and_block(DBusConnection *conn, DBusMessage *call) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *resp = dbus_connection_send_with_reply_and_block(conn, call, DBUS_TIMEOUT_USE_DEFAULT, &error);
	dbus_message_unref(call);
	if (resp == NULL) {
		fprintf(stderr, "dbus_connection_send_with_reply_and_block: %s\n", error.message);
		return NULL;
	}
	if (dbus_error_is_set(&error)) {
		return NULL;
	}
	return resp;
}

static int get_dbus_property(DBusConnection *conn, const char *dest, const char *path, const char *iface, const char *prop, DBusMessage **resp) {
	DBusError error;
	dbus_error_init(&error);
	DBusMessage *call = dbus_message_new_method_call(dest, path, "org.freedesktop.DBus.Properties", "Get");
	dbus_message_append_args(call, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);
	(*resp) = dbus_connection_send_with_reply_and_block(conn, call, DBUS_TIMEOUT_USE_DEFAULT, &error);
	dbus_message_unref(call);
	if (resp == NULL) return -1;
	if (dbus_error_is_set(&error)) return -1;
	return 0;
}

static int get_u8array(DBusMessageIter *iter, char *val, unsigned int max) {
	if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_ARRAY) {
		DBusMessageIter dict;
		dbus_message_iter_recurse(iter, &dict);
		int current_type;
		int i = 0;
		while ((current_type = dbus_message_iter_get_arg_type(&dict)) != DBUS_TYPE_INVALID) {
			uint8_t c;
			dbus_message_iter_get_basic(&dict, &c);
			val[i] = (char)c;
			dbus_message_iter_next(&dict);
			i++;
			if (i >= max) return -1;
		}
		val[i] = '\0';
		return 0;
	}
	return -1;
}
