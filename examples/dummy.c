#include <stdlib.h>
#include <runtime.h>
#include <wifi.h>

struct ModulePriv {
	int x;
};

static int connected(struct PakNet *ctx, struct PakWiFiAdapter *adapter, void *arg) {
	struct Module *mod = arg;
	mod->on_try_connect_wifi(mod, adapter, -1);
}

static int manual_connect(struct Module *mod) {
	struct PakNet *ctx = pak_net_get_context();
	struct PakWiFiApFilter spec = {
		.has_ssid = 1,
		.ssid_pattern = "V300.*",
		.has_password = 1,
		.password = "12345678",
	};
	pak_wifi_request_connection(ctx, &spec, connected, mod);
	return 0;
}

static int init(struct Module *mod) {
	mod->priv = (struct ModulePriv *)malloc(sizeof(struct ModulePriv));

	struct Manifest m = {
		.name = "Dummy module",
		.description = "A dummy module that doesn't do much",
		.author = "Daniel Cook",
	};
	
	return 0;
}

static int on_try_connect_wifi(struct Module *mod, struct PakWiFiAdapter *handle, int job) {
	return 0;
}

static int on_idle_tick(struct Module *mod, unsigned int us_since_last_tick) {
	return 0;
}

static int on_disconnect(struct Module *mod) {
	return 0;
}

static int on_switch_screen(struct Module *mod, int old_screen, int new_screen, int job) {
	return 0;
}

static int on_request_file_contents(struct Module *, int screen, int job, struct FileHandle *file) {
	return 0;
}

static int on_request_thumbnail(struct Module *, int screen, int job, struct FileHandle *file) {
	return 0;
}

static int on_request_file_metadata(struct Module *, int screen, int job, struct FileHandle *file) {
	return 0;
}

static int on_custom_command(struct Module *, const char *request) {
	return 0;
}

int get_module_dummy(struct Module *mod) {
	mod->init = init;
	mod->on_try_connect_wifi = on_try_connect_wifi;
	mod->on_idle_tick = on_idle_tick;
	mod->on_disconnect = on_disconnect;
	mod->on_switch_screen = on_switch_screen;
}
