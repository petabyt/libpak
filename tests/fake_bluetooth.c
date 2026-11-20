/* Deterministic, single-threaded Bluetooth backend for tests. It does not use
 * D-Bus or radio hardware. Each context has one adapter, device and GATT
 * characteristic; handles are separately allocated snapshots. */
#include <stdlib.h>
#include <string.h>
#include <bluetooth.h>

struct PakBt {
	int connected, bonded, watching;
	unsigned char value[512];
	unsigned int length;
	struct PakBtDevice *listener_device;
	pak_bt_listen_device *listener;
	void *listener_arg;
};

static int valid(struct PakBt *ctx, void *object) { return ctx && object; }
static void event(struct PakBt *ctx, enum PakBtEvent type, struct PakGattCharacteristic *chr) {
	if (ctx->listener) ctx->listener(ctx, type, ctx->listener_device, chr, ctx->listener_arg);
}

struct PakBt *pak_bt_get_context(void) { return calloc(1, sizeof(struct PakBt)); }
void pak_bt_unref_context(struct PakBt *ctx) { free(ctx); }
int pak_bt_is_enabled(struct PakBt *ctx) { return ctx ? 1 : 0; }
int pak_bt_is_supported(struct PakBt *ctx, enum PakBtFeature feature) { return ctx && feature == PAK_SUPPORT_BLE; }
int pak_bt_get_n_adapters(struct PakBt *ctx) { return ctx ? 1 : -1; }
struct PakBtAdapter *pak_bt_get_adapter(struct PakBt *ctx, int index) {
	if (!ctx || index != 0) return NULL;
	struct PakBtAdapter *a = calloc(1, sizeof(*a));
	if (a) { strcpy(a->name, "Fake Bluetooth"); strcpy(a->address, "02:00:00:00:00:01"); a->powered = 1; }
	return a;
}
int pak_bt_unref_adapter(struct PakBt *ctx, struct PakBtAdapter *a) { if (!valid(ctx, a)) return -1; free(a); return 0; }
static int matches(struct PakBt *ctx, int filter) {
	return (!(filter & PAK_FILTER_BONDED) || ctx->bonded) && (!(filter & PAK_FILTER_CONNECTED) || ctx->connected);
}
int pak_bt_get_n_devices(struct PakBt *ctx, struct PakBtAdapter *a, int filter) {
	return valid(ctx, a) ? matches(ctx, filter) : -1;
}
struct PakBtDevice *pak_bt_get_device(struct PakBt *ctx, struct PakBtAdapter *a, int index, int filter) {
	if (!valid(ctx, a) || index != 0 || !matches(ctx, filter)) return NULL;
	struct PakBtDevice *d = calloc(1, sizeof(*d));
	if (d) { strcpy(d->name, "Fake BLE device"); strcpy(d->mac_address, "02:00:00:00:00:02"); d->is_connected = ctx->connected; d->is_bonded = ctx->bonded; }
	return d;
}
int pak_bt_unref_device(struct PakBt *ctx, struct PakBtDevice *d) {
	if (!valid(ctx, d)) return -1;
	if (ctx->listener_device == d) { ctx->listener = NULL; ctx->listener_device = NULL; ctx->listener_arg = NULL; }
	free(d); return 0;
}
int pak_bt_device_update(struct PakBt *ctx, struct PakBtDevice *d) {
	if (!valid(ctx, d)) return -1;
	d->is_connected = ctx->connected; d->is_bonded = ctx->bonded; return 0;
}
int pak_bt_set_device_callback(struct PakBt *ctx, struct PakBtDevice *d, pak_bt_listen_device *cb, void *arg) {
	if (!valid(ctx, d)) return -1;
	if (!cb && ctx->listener_device != d) return 0;
	ctx->listener_device = cb ? d : NULL; ctx->listener = cb; ctx->listener_arg = cb ? arg : NULL; return 0;
}
int pak_bt_device_connect(struct PakBt *ctx, struct PakBtDevice *d, int blocking) {
	(void)blocking; if (!valid(ctx, d)) return -1;
	if (!ctx->connected) { ctx->connected = 1; pak_bt_device_update(ctx, d); event(ctx, PAK_BT_EVENT_CONNECTED, NULL); event(ctx, PAK_BT_EVENT_SERVICES_DISCOVERED, NULL); }
	return 0;
}
int pak_bt_device_disconnect(struct PakBt *ctx, struct PakBtDevice *d) {
	if (!valid(ctx, d)) return -1;
	if (ctx->connected) { ctx->connected = 0; ctx->watching = 0; pak_bt_device_update(ctx, d); event(ctx, PAK_BT_EVENT_DISCONNECTED, NULL); }
	return 0;
}
int pak_bt_device_create_bond(struct PakBt *ctx, struct PakBtDevice *d, int blocking) {
	(void)blocking; if (!valid(ctx, d)) return -1; ctx->bonded = 1; return pak_bt_device_update(ctx, d);
}
unsigned int pak_bt_get_manufacturer_data(struct PakBt *ctx, struct PakBtDevice *d, int index, void *buf, unsigned int max) {
	static const uint8_t data[] = { 0x34, 0x12, 0x42 };
	if (!valid(ctx, d) || index || (!buf && max)) return 0;
	unsigned int n = max < sizeof(data) ? max : sizeof(data); if (n) memcpy(buf, data, n); return n;
}
struct PakGattService *pak_bt_get_gatt_service(struct PakBt *ctx, struct PakBtDevice *d, int index) {
	if (!valid(ctx, d) || !ctx->connected || index) return NULL;
	struct PakGattService *s = calloc(1, sizeof(*s));
	if (s) { strcpy(s->uuid, "0000180f-0000-1000-8000-00805f9b34fb"); s->handle = 1; }
	return s;
}
int pak_bt_unref_gatt_service(struct PakBt *ctx, struct PakGattService *s) { if (!valid(ctx, s)) return -1; free(s); return 0; }
struct PakGattCharacteristic *pak_bt_get_gatt_characteristic(struct PakBt *ctx, struct PakGattService *s, int index) {
	if (!valid(ctx, s) || !ctx->connected || index) return NULL;
	struct PakGattCharacteristic *c = calloc(1, sizeof(*c));
	if (c) { strcpy(c->uuid, "00002a19-0000-1000-8000-00805f9b34fb"); c->handle = 2; c->mtu = 23; }
	return c;
}
int pak_bt_unref_gatt_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *c) { if (!valid(ctx, c)) return -1; free(c); return 0; }
int pak_bt_read_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *c, int blocking) {
	(void)blocking; if (!valid(ctx, c) || !ctx->connected) return PAK_ERR_NO_CONNECTION;
	event(ctx, PAK_BT_EVENT_GATT_CHAR_READ, c); return 0;
}
int pak_bt_write_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *c, const void *data, unsigned int len, int blocking) {
	(void)blocking; if (!valid(ctx, c) || !ctx->connected) return PAK_ERR_NO_CONNECTION;
	if (len > sizeof(ctx->value) || (!data && len)) return -1;
	if (len) memcpy(ctx->value, data, len);
	ctx->length = len;
	event(ctx, PAK_BT_EVENT_GATT_CHAR_WRITTEN, c);
	if (ctx->watching) event(ctx, PAK_BT_EVENT_GATT_CHAR_CHANGED, c);
	return 0;
}
unsigned int pak_bt_read_characteristic_cached_value(struct PakBt *ctx, struct PakGattCharacteristic *c, void *buf, unsigned int max) {
	if (!valid(ctx, c) || (!buf && max)) return 0;
	unsigned int n = max < ctx->length ? max : ctx->length; if (n) memcpy(buf, ctx->value, n); return n;
}
int pak_bt_set_watching_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *c, int v) {
	if (!valid(ctx, c) || !ctx->connected) return PAK_ERR_NO_CONNECTION;
	ctx->watching = !!v;
	return 0;
}
int pak_bt_watch_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *c, unsigned int ms) {
	(void)ms; return valid(ctx, c) && ctx->connected ? 0 : PAK_ERR_NO_CONNECTION;
}
int pak_bt_set_cccd(struct PakBt *ctx, struct PakGattCharacteristic *c, int v) { return pak_bt_set_watching_characteristic(ctx, c, v); }
struct PakBtSocket *pak_bt_connect_to_service_channel(struct PakBt *ctx, struct PakBtDevice *d, const char *uuid, int flags) { (void)ctx; (void)d; (void)uuid; (void)flags; return NULL; }
int pak_bt_write(struct PakBtSocket *s, const void *data, unsigned int len) { (void)s; (void)data; (void)len; return PAK_ERR_UNSUPPORTED; }
int pak_bt_read(struct PakBtSocket *s, void *data, unsigned int len) { (void)s; (void)data; (void)len; return PAK_ERR_UNSUPPORTED; }
int pak_bt_close_socket(struct PakBtSocket *s) { (void)s; return PAK_ERR_UNSUPPORTED; }
