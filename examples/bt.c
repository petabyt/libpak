#include <stdlib.h>
#include <runtime.h>
#include <wifi.h>
#include <bluetooth.h>

int main(void) {
	struct PakBt *ctx = pak_bt_get_context();

	int len = pak_bt_get_n_adapters(ctx);
	if (len <= 0) return -1;
	struct PakBtAdapter adapter;
	if (pak_bt_get_adapter(ctx, &adapter, 0)) return -1;

	struct PakBtDevice dev;
	int i = 0;
	int rc = pak_bt_get_device(ctx, &adapter, &dev, i, PAK_FILTER_CONNECTED);
	if (rc) return rc;

	rc = pak_bt_device_connect(ctx, &dev);
	if (rc) {
		printf("pak_bt_device_connect\n");
		return rc;
	}

	if (!dev.is_classic) {
		printf("Mfgdata: {");
		for (int z = 0; z < 0x10; z++) {
			printf("%02x,", dev.mfg_data[z]);
		}
		printf("}\n");

		struct PakGattService service;
		struct PakGattCharacteristic chr;
		pak_bt_get_gatt_service(ctx, &dev, &service, 0);
		pak_bt_get_gatt_characteristic(ctx, &service, &chr, 0);

		printf("CHR: %s\n", chr.uuid);

		uint8_t data[] = {1};
		pak_bt_write_characteristic(ctx, &chr, data, sizeof(data), 1);

		pak_bt_unref_gatt_characteristic(ctx, &chr);
		pak_bt_unref_gatt_service(ctx, &service);
	}

	pak_bt_unref_device(ctx, &dev);

	pak_bt_unref_adapter(ctx, &adapter);

	return 0;
}
