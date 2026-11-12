#include <stdbool.h>
#include <unistd.h>
#include <runtime.h>
#include <wifi.h>

static int on_find_connection(struct PakModule *mod, int job) {
	return -1;
}

static int init(struct PakModule *mod) {
	//pak_debug_log(mod, "Hello from dummy module");
	return 0;
}

static int on_try_connect_wifi(struct PakModule *mod, struct PakWiFiAdapter *handle, int job) {
	return -1;
}

static int on_idle_tick(struct PakModule *mod, unsigned int us_since_last_tick) {
	return 0;
}

static int on_disconnect(struct PakModule *mod) {
	return 0;
}

static int on_switch_screen(struct PakModule *mod, int old_screen, int new_screen, int job) {
	return 0;
}

static int on_run_test(struct PakModule *mod, int screen, int job) {
	return 0;
}

int get_module(struct PakModule *mod) {
	pak_global_log("Test\n");
	mod->init = init;
	mod->on_find_connection = on_find_connection;
	mod->on_idle_tick = on_idle_tick;
	mod->on_disconnect = on_disconnect;
	mod->on_switch_screen = on_switch_screen;
	return 0;
}
