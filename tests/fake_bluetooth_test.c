#include <assert.h>
#include <string.h>
#include <bluetooth.h>

static int events[8], count;
static int callback(struct PakBt *ctx, enum PakBtEvent type, struct PakBtDevice *dev,
		struct PakGattCharacteristic *chr, void *arg) {
	assert(ctx && dev && arg == &count);
	if (type == PAK_BT_EVENT_GATT_CHAR_WRITTEN || type == PAK_BT_EVENT_GATT_CHAR_CHANGED) {
		char buffer[3];
		assert(chr && pak_bt_read_characteristic_cached_value(ctx, chr, buffer, sizeof(buffer)) == 3);
		assert(!memcmp(buffer, "abc", 3));
	}
	assert(count < 8); events[count++] = type; return 0;
}
int main(void) {
	struct PakBt *ctx = pak_bt_get_context(); assert(ctx);
	struct PakBtAdapter *a = pak_bt_get_adapter(ctx, 0); assert(a);
	assert(pak_bt_get_n_devices(ctx, a, PAK_FILTER_CONNECTED) == 0);
	struct PakBtDevice *d = pak_bt_get_device(ctx, a, 0, 0); assert(d);
	assert(!pak_bt_set_device_callback(ctx, d, callback, &count));
	assert(!pak_bt_device_connect(ctx, d, PAK_BLOCK));
	assert(pak_bt_get_n_devices(ctx, a, PAK_FILTER_CONNECTED) == 1);
	struct PakGattService *s = pak_bt_get_gatt_service(ctx, d, 0); assert(s);
	struct PakGattCharacteristic *c = pak_bt_get_gatt_characteristic(ctx, s, 0); assert(c);
	assert(!pak_bt_set_watching_characteristic(ctx, c, 1));
	assert(!pak_bt_write_characteristic(ctx, c, "abc", 3, PAK_BLOCK));
	assert(!pak_bt_read_characteristic(ctx, c, PAK_BLOCK));
	assert(!pak_bt_device_disconnect(ctx, d));
	const int expected[] = { PAK_BT_EVENT_CONNECTED, PAK_BT_EVENT_SERVICES_DISCOVERED,
		PAK_BT_EVENT_GATT_CHAR_WRITTEN, PAK_BT_EVENT_GATT_CHAR_CHANGED,
		PAK_BT_EVENT_GATT_CHAR_READ, PAK_BT_EVENT_DISCONNECTED };
	assert(count == 6 && !memcmp(events, expected, sizeof(expected)));
	assert(pak_bt_set_device_callback(ctx, d, NULL, NULL) == -1);
	assert(!pak_bt_device_connect(ctx, d, PAK_BLOCK) && count == 8);
	assert(!pak_bt_unref_gatt_characteristic(ctx, c));
	assert(!pak_bt_unref_gatt_service(ctx, s));
	assert(!pak_bt_unref_device(ctx, d));
	assert(!pak_bt_unref_adapter(ctx, a));
	pak_bt_unref_context(ctx);
	return 0;
}
