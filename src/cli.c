#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "wifi.h"
#include "bluetooth.h"
#include "runtime.h"
#include "runtime_ext.h"
#include "main.h"

int test_bluetooth(void) {
	struct PakBt *ctx = pak_bt_get_context();

	int len = pak_bt_get_n_adapters(ctx);
	if (len <= 0) return -1;
	struct PakBtAdapter *adapter = pak_bt_get_adapter(ctx, 0);
	if (adapter == NULL) return -1;

	int i = 0;
	struct PakBtDevice *dev;
	while ((dev = pak_bt_get_device(ctx, adapter, i, PAK_FILTER_BONDED)) != NULL) {
		printf("Paired device: %s\n", dev->name);
		if (!dev->is_classic) {
			uint8_t buf[0xff];
			unsigned int sz = pak_bt_get_manufacturer_data(ctx, dev, 0, buf, sizeof(buf));
			printf("Mfgdata: {");
			for (int z = 0; z < sz; z++) {
				printf("%02x,", buf[z]);
			}
			printf("}\n");

			struct PakGattService *service;
			struct PakGattCharacteristic *chr;
			for (int x = 0; !(service = pak_bt_get_gatt_service(ctx, dev, x)); x++) {
				printf("Service: %s\n", service->uuid);
				for (int i = 0; !(chr = pak_bt_get_gatt_characteristic(ctx, service, i)); i++) {
					printf("  Characteristic: %s\n", chr->uuid);
					pak_bt_unref_gatt_characteristic(ctx, chr);
				}
				pak_bt_unref_gatt_service(ctx, service);
			}
		}

		i++;
		pak_bt_unref_device(ctx, dev);
	}

	//pak_main_loop();

	pak_bt_unref_adapter(ctx, adapter);

	return 0;
}

int test_wifi(void) {
	struct PakNet *ctx = pak_net_get_context();

	int len = pak_wifi_get_n_adapters(ctx);
	if (len <= 0) return -1;
	struct PakWiFiAdapter *adapter = pak_wifi_get_adapter(ctx, 0);
	if (adapter == NULL) return -1;
	printf("WiFi adapter: %s\n", adapter->name);

	int n_aps = pak_wifi_get_n_aps(ctx, adapter);
	for (int i = 0; i < n_aps; i++) {
		struct PakWiFiAp *ap;
		if ((ap = pak_wifi_get_ap(ctx, adapter, i))) return -1;
		printf("ssid: %s\n", ap->ssid);
		pak_wifi_unref_ap(ctx, adapter, ap);
	}

	pak_main_loop();

	pak_wifi_unref_adapter(ctx, adapter);

	return 0;
}

int test_bluetooth_connect(void) {
	struct PakBt *ctx = pak_bt_get_context();

	int len = pak_bt_get_n_adapters(ctx);
	if (len <= 0) return -1;
	struct PakBtAdapter *adapter = pak_bt_get_adapter(ctx, 0);
	if (adapter == NULL) return -1;

	struct PakBtDevice *dev;
	dev = pak_bt_get_device(ctx, adapter, 0, PAK_FILTER_CONNECTED);
	printf("Saved device: %s\n", dev->name);

	struct PakGattService *service;
	struct PakGattCharacteristic *chr;
	for (int x = 0; !(service = pak_bt_get_gatt_service(ctx, dev, x)); x++) {
		printf("Service: %s\n", service->uuid);
		for (int i = 0; !pak_bt_get_gatt_characteristic(ctx, service, i); i++) {
			printf("  Characteristic: %s\n", chr->uuid);
			pak_bt_unref_gatt_characteristic(ctx, chr);
		}
		pak_bt_unref_gatt_service(ctx, service);
	}

	pak_bt_unref_adapter(ctx, adapter);

	return 0;
}

int help(void) {
	printf("--test-js <js file>\n");
	printf("--dump-bt\n");
	return 0;
}

static char *alloc_file(const char *filename, unsigned int *len) {
	FILE *file = fopen(filename, "rb");
	if (!file) {
		printf("Failed to open '%s'\n", filename);
		return NULL;
	}
	
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *buffer = (char *)malloc(file_size + 1);
	if (!buffer) return NULL;
	
	fread(buffer, 1, file_size, file);
	buffer[file_size] = '\0';

	fclose(file);
	(*len) = file_size;
	return buffer;
}

int main(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--test-js")) {
			struct Module *mod = pak_create_mod();
			unsigned int len = 0;
			char *buf = alloc_file(argv[i + 1], &len);
			if (buf == NULL) return -1;
			if (setup_quickjs_module(mod, buf, len)) return -1;
			return pak_rt_test_module(mod);
		} else if (!strcmp(argv[i], "--test-wasm")) {
			struct Module *mod = pak_create_mod();
			unsigned int len = 0;
			char *buf = alloc_file(argv[i + 1], &len);
			if (buf == NULL) return -1;
			if (setup_wasm_module(mod, buf, len)) return -1;
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
