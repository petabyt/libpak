#include <runtime.h>
#include <stdatomic.h>

static _Atomic int events;

static int listener(struct PakBt *ctx, enum PakBtEvent type, struct PakBtDevice *dev,
		struct PakGattCharacteristic *chr, void *arg) {
	(void)chr;
	if (!ctx || !dev || arg != (void *)123 || !pak_bt_is_enabled(ctx)) return -1;
	if (type == PAK_BT_EVENT_CONNECTED || type == PAK_BT_EVENT_SERVICES_DISCOVERED)
		atomic_fetch_add(&events, 1);
	return 0;
}

static int run(struct PakModule *mod, int job) {
	(void)job;
	struct PakBtAdapter *a = pak_bt_get_adapter(mod->bt, 0);
	struct PakBtDevice *d = a ? pak_bt_get_device(mod->bt, a, 0, 0) : 0;
	if (!d) return 1;
	if (pak_bt_set_device_callback(mod->bt, d, listener, (void *)123)) return 2;
	if (pak_bt_set_device_callback(mod->bt, d, NULL, NULL) != -1) return 3;
	if (pak_bt_device_connect(mod->bt, d, PAK_BLOCK)) return 4;
	for (unsigned i = 0; i < 100000000; i++) {
		if (atomic_load(&events) == 2) return 0;
	}
	return 5;
}

int get_module(struct PakModule *m) { m->on_run_test = run; return 0; }
