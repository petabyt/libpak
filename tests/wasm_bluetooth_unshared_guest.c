#include <runtime.h>

static int listener(struct PakBt *ctx, enum PakBtEvent type, struct PakBtDevice *dev,
		struct PakGattCharacteristic *chr, void *arg) {
	(void)ctx; (void)type; (void)dev; (void)chr; (void)arg;
	return 0;
}

static int run(struct PakModule *mod, int job) {
	(void)job;
	struct PakBtAdapter *a = pak_bt_get_adapter(mod->bt, 0);
	struct PakBtDevice *d = a ? pak_bt_get_device(mod->bt, a, 0, 0) : 0;
	if (!d) return 1;
	return pak_bt_set_device_callback(mod->bt, d, listener, NULL) == -1 ? 0 : 2;
}

int get_module(struct PakModule *m) { m->on_run_test = run; return 0; }
