// dbus helpers
#include <stdlib.h>
#include <dbus/dbus.h>

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
