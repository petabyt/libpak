#include <runtime.h>
#include <string.h>
#include <stddef.h>

_Static_assert(sizeof(struct PakBtAdapter) == 136, "wasm32 adapter ABI");
_Static_assert(sizeof(struct PakBtDevice) == 140 && offsetof(struct PakBtDevice, btclass) == 136, "wasm32 device ABI");
_Static_assert(sizeof(struct PakGattService) == 44 && offsetof(struct PakGattService, handle) == 42, "wasm32 service ABI");
_Static_assert(sizeof(struct PakGattCharacteristic) == 52 && offsetof(struct PakGattCharacteristic, mtu) == 48, "wasm32 characteristic ABI");

static int events[16], count, callback_error;
static int listener(struct PakBt *ctx, enum PakBtEvent event, struct PakBtDevice *dev,
		struct PakGattCharacteristic *chr, void *arg) {
	if (!ctx || !dev || arg != (void *)0x1234 || count >= 16) return -1;
	events[count++] = event;
	if (event == PAK_BT_EVENT_GATT_CHAR_WRITTEN || event == PAK_BT_EVENT_GATT_CHAR_CHANGED) {
		unsigned char out[4] = {0};
		if (!chr || pak_bt_read_characteristic_cached_value(ctx, chr, out, sizeof(out)) != 3 || memcmp(out, "abc", 3)) callback_error = 1;
	}
	return 0;
}
static int run(struct PakModule *mod, int job) {
	(void)job;
	struct PakBt *ctx = mod->bt;
	if (!ctx || pak_bt_get_context() != ctx || !pak_bt_is_enabled(ctx)) return 1;
	if (pak_bt_get_n_adapters(ctx) != 1) return 2;
	struct PakBtAdapter *a = pak_bt_get_adapter(ctx, 0);
	if (!a || strcmp(a->name, "Fake Bluetooth") || pak_bt_get_n_devices(ctx, a, 0) != 1) return 3;
	struct PakBtDevice *d = pak_bt_get_device(ctx, a, 0, 0);
	if (!d || strcmp(d->name, "Fake BLE device")) return 4;
	if (pak_bt_set_device_callback(ctx, d, listener, (void *)0x1234)) return 5;
	if (pak_bt_device_connect(ctx, d, PAK_BLOCK) || !d->is_connected) return 6;
	struct PakGattService *s = pak_bt_get_gatt_service(ctx, d, 0);
	if (!s || strcmp(s->uuid, "0000180f-0000-1000-8000-00805f9b34fb")) return 7;
	struct PakGattCharacteristic *c = pak_bt_get_gatt_characteristic(ctx, s, 0);
	if (!c || c->handle != 2 || c->mtu != 23) return 8;
	if (pak_bt_set_watching_characteristic(ctx, c, 1)) return 9;
	if (pak_bt_write_characteristic(ctx, c, "abc", 3, PAK_BLOCK)) return 10;
	if (pak_bt_read_characteristic(ctx, c, PAK_BLOCK)) return 11;
	if (pak_bt_device_disconnect(ctx, d)) return 12;
	const int expected[] = { PAK_BT_EVENT_CONNECTED, PAK_BT_EVENT_SERVICES_DISCOVERED,
		PAK_BT_EVENT_GATT_CHAR_WRITTEN, PAK_BT_EVENT_GATT_CHAR_CHANGED,
		PAK_BT_EVENT_GATT_CHAR_READ, PAK_BT_EVENT_DISCONNECTED };
	if (callback_error || count != 6 || memcmp(events, expected, sizeof(expected))) return 13;
	if (pak_bt_device_connect(ctx, d, PAK_BLOCK) || count != 8) return 16;
	if (pak_bt_set_watching_characteristic(ctx, c, 1)) return 19;
	if (pak_bt_set_device_callback(ctx, d, NULL, NULL) != -1) return 20;
	if (pak_bt_write_characteristic(ctx, c, "abc", 3, PAK_BLOCK) || count != 10 || callback_error) return 24;
	if (pak_bt_get_n_devices(ctx, (struct PakBtAdapter *)c, 0) >= 0) return 17;
	if (pak_bt_unref_gatt_characteristic(ctx, c) || pak_bt_unref_gatt_service(ctx, s) ||
		pak_bt_unref_device(ctx, d) || pak_bt_unref_adapter(ctx, a)) return 15;
	if (!pak_bt_unref_device(ctx, d)) return 18;
	/* Exercise teardown with live objects and a registered callback. */
	a = pak_bt_get_adapter(ctx, 0); if (!a) return 21;
	d = pak_bt_get_device(ctx, a, 0, 0); if (!d) return 22;
	if (pak_bt_set_device_callback(ctx, d, listener, (void *)0x1234)) return 23;
	return 0;
}
int get_module(struct PakModule *m) { m->on_run_test = run; return 0; }
