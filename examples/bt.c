#include <stdlib.h>
#include <runtime.h>
#include <wifi.h>
#include <bluetooth.h>

int main(void) {
	struct PakBt *ctx = pak_bt_get_context();

	int len = pak_bt_get_n_adapters(ctx);
	if (len <= 0) return -1;
	struct PakBtAdapter *adapter = pak_bt_get_adapter(ctx, 0);
	if (adapter == NULL) return -1;

	struct PakBtDevice *dev = pak_bt_get_device(ctx, adapter, 0, PAK_FILTER_CONNECTED);
	if (dev == NULL) return -1;

	int rc = pak_bt_device_connect(ctx, dev);
	if (rc) {
		printf("pak_bt_device_connect\n");
		return rc;
	}

	if (!dev->is_classic) {
		uint8_t buf[0xff];
		unsigned int sz = pak_bt_get_manufacturer_data(ctx, dev, 0, buf, sizeof(buf));
		printf("Mfgdata: {");
		for (int z = 0; z < sz; z++) {
			printf("%02x,", buf[z]);
		}
		printf("}\n");

		struct PakGattService *service = pak_bt_get_gatt_service(ctx, dev, 0);
		struct PakGattCharacteristic *chr = pak_bt_get_gatt_characteristic(ctx, service, 0);

		printf("CHR: %s\n", chr->uuid);

		uint8_t data[] = {1};
		pak_bt_write_characteristic(ctx, chr, data, sizeof(data), 1);

		pak_bt_unref_gatt_characteristic(ctx, chr);
		pak_bt_unref_gatt_service(ctx, service);
	}

	pak_bt_unref_device(ctx, dev);

	pak_bt_unref_adapter(ctx, adapter);

	return 0;
}
