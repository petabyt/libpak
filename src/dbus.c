#include <dbus/dbus.h>
#include "dbus.h"

int pak_main_loop(void) {
	DBusConnection *conn = get_dbus_system();
	while (1) {
		dbus_connection_read_write_dispatch(conn, 1000);
	}
}
