#include <stdio.h>
//#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <quickjs.h>
#include <quickjs-libc.h>
#include "wifi.h"
#include "bluetooth.h"
#include "runtime.h"

int get_module_dummy(struct Module *mod);

/// Execute JS file and get the exported module handle
int setup_quickjs_module(struct Module **mod, const char *filename);

int pak_rt_test_module(struct Module *mod);

int test_bluetooth(void) {
	struct PakBt *ctx = pak_bt_get_context();

	int len = pak_bt_get_n_adapters(ctx);
	if (len <= 0) return -1;
	struct PakBtAdapter adapter;
	if (pak_bt_get_adapter(ctx, &adapter, 0)) return -1;

	struct PakBtDevice dev;
	int i = 0;
	while (pak_bt_get_device(ctx, &adapter, &dev, i, PAK_FILTER_BONDED) == 0) {
		printf("Paired device: %s\n", dev.name);

//		int percent;
//		if (!pak_bt_get_device_battery(ctx, &dev, &percent)) {
//			printf("Battery: %d%%\n", percent);
//		}

		printf("Service UUIDs:\n");
		for (int z = 0; z < dev.uuids.length; z++) {
			char uuid[37];
			printf("  %s\n", dev.uuids.uuids[z]);
		}
		if (!dev.is_classic) {
			printf("Mfgdata: {");
			for (int z = 0; z < 0x10; z++) {
				printf("%02x,", dev.mfg_data[z]);
			}
			printf("}\n");

			struct PakGattService service;
			struct PakGattCharacteristic chr;
			for (int x = 0; !pak_bt_get_gatt_service(ctx, &dev, &service, x); x++) {
				printf("Service: %s\n", service.uuid);
				for (int i = 0; !pak_bt_get_gatt_characteristic(ctx, &service, &chr, i); i++) {
					printf("  Characteristic: %s\n", chr.uuid);
					pak_bt_unref_gatt_characteristic(ctx, &chr);
				}
				pak_bt_unref_gatt_service(ctx, &service);
			}
		}

		i++;
		pak_bt_unref_device(ctx, &dev);
	}

	//pak_main_loop();

	pak_bt_unref_adapter(ctx, &adapter);

	return 0;
}

int test_wifi(void) {
	struct PakNet *ctx = pak_net_get_context();

	int len = pak_wifi_get_n_adapters(ctx);
	if (len <= 0) return -1;
	struct PakWiFiAdapter adapter;
	if (pak_wifi_get_adapter(ctx, &adapter, 0)) return -1;
	printf("WiFi adapter: %s\n", adapter.name);

	int n_aps = pak_wifi_get_n_aps(ctx, &adapter);
	struct PakWiFiAp ap;
	for (int i = 0; i < n_aps; i++) {
		if (pak_wifi_get_ap(ctx, &adapter, &ap, i)) return -1;
		printf("ssid: %s\n", ap.ssid);
		pak_wifi_unref_ap(ctx, &adapter, &ap);
	}

	pak_main_loop();

	pak_wifi_unref_adapter(ctx, &adapter);

	return 0;
}

int test_bluetooth_connect(void) {
	struct PakBt *ctx = pak_bt_get_context();

	int len = pak_bt_get_n_adapters(ctx);
	if (len <= 0) return -1;
	struct PakBtAdapter adapter;
	if (pak_bt_get_adapter(ctx, &adapter, 0)) return -1;

	struct PakBtDevice dev;
	int rc = pak_bt_get_device(ctx, &adapter, &dev, 0, PAK_FILTER_CONNECTED);
	printf("Saved device: %s\n", dev.name);

	struct PakGattService service;
	struct PakGattCharacteristic chr;
	for (int x = 0; !pak_bt_get_gatt_service(ctx, &dev, &service, x); x++) {
		printf("Service: %s\n", service.uuid);
		for (int i = 0; !pak_bt_get_gatt_characteristic(ctx, &service, &chr, i); i++) {
			printf("  Characteristic: %s\n", chr.uuid);
			pak_bt_unref_gatt_characteristic(ctx, &chr);
		}
		pak_bt_unref_gatt_service(ctx, &service);
	}

	pak_bt_unref_adapter(ctx, &adapter);

	return 0;
}

int help(void) {
	printf("--test-js <js file>\n");
	printf("--dump-bt\n");
	return 0;
}

int main(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--test-js")) {
			struct Module *mod;
			if (setup_quickjs_module(&mod, argv[i + 1])) return -1;
			return pak_rt_test_module(mod);
		} else if (!strcmp(argv[i], "--test")) {
			int rc = test_wifi();
			rc |= test_bluetooth();
			return rc;
		} else if (!strcmp(argv[i], "--dump-bt")) {
			return test_bluetooth();
		} else if (!strcmp(argv[i], "--bt-con")) {
			return test_bluetooth_connect();
		} else if (!strcmp(argv[i], "--test-dummy-mod")) {
			return pak_rt_test_module(pak_rt_mod_from_native(get_module_dummy));
		} else {
			printf("Invalid argument '%s'\n", argv[i]);
		}
	}
	return help();
}
