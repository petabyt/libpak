#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>
#include "wifi.h"
#include "bluetooth.h"
#include "runtime.h"

int get_module_dummy(struct Module *mod);

JSModuleDef *js_init_module_socket(JSContext *ctx, const char *module_name);
JSModuleDef *js_init_module_wifi(JSContext *ctx, const char *module_name);
JSModuleDef *js_init_module_pak_runtime(JSContext *ctx, const char *module_name);

int run_quickjs(const char *filename) {
	JSRuntime *rt = JS_NewRuntime();

	JSContext *ctx = JS_NewContext(rt);
	js_std_add_helpers(ctx, 0, NULL);

	JS_AddModuleExport(ctx, js_init_module_wifi(ctx, "pak:wifi"), "WiFi");
	JS_AddModuleExport(ctx, js_init_module_pak_runtime(ctx, "pak:runtime"), "Module");
	js_init_module_socket(ctx, "c:socket");
	js_init_module_std(ctx, "qjs:std");
	JS_SetModuleLoaderFunc(rt, NULL, js_module_loader, NULL);
	js_std_init_handlers(rt);

	FILE *file = fopen(filename, "rb");
	if (!file) {
		printf("Failed to open '%s'\n", filename);
		return -1;
	}
	
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *buffer = (char *)malloc(file_size + 1);
	if (!buffer) return -1;
	
	fread(buffer, 1, file_size, file);
	buffer[file_size] = '\0';

	fclose(file);

	JSValue val = JS_Eval(ctx, buffer, file_size, filename, JS_EVAL_TYPE_MODULE);
	if (JS_IsException(val)) {
		const char *str = JS_ToCString(ctx, val);
		printf("JS error: %s\n", str);
		JS_FreeCString(ctx, str);
		return -1;
	}

	JS_FreeValue(ctx, val);

	js_std_free_handlers(rt);
	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);

	return 0;
}

int test_bluetooth(void) {
	struct PakBt *ctx = pak_bt_get_context();

	int len = pak_bt_get_n_adapters(ctx);
	if (len <= 0) return -1;
	struct PakBtAdapter adapter;
	if (pak_bt_get_adapter(ctx, &adapter, 0)) return -1;

	struct PakBtDevice dev;
	int i = 0;
	while (pak_bt_get_paired_device(ctx, &adapter, &dev, i) == 0) {
		printf("Paired device: %s\n", dev.name);
		for (int z = 0; z < dev.uuids.length; z++) {
			char uuid[37];
			pak_uuid128_to_str(dev.uuids.uuids[z], uuid);
			printf("%s\n", uuid);
		}
		printf("Mfgdata: {");
		for (int z = 0; z < 0x10; z++) {
			printf("%02x,", dev.mfg_data[z]);
		}
		printf("}\n");

		int percent;
		if (pak_bt_get_device_battery(ctx, &dev, &percent)) return -1;
		printf("Battery: %d%%\n", percent);

		i++;
		pak_bt_unref_device(ctx, &dev);
	}

	pak_main_loop();

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

int pak_rt_test_module(struct Module *mod);

int main(int argc, char **argv) {
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--js")) {
			return run_quickjs(argv[i + 1]);
		} else if (!strcmp(argv[i], "--test")) {
			int rc = test_wifi();
			rc |= test_bluetooth();
			return rc;
		} else if (!strcmp(argv[i], "--dump-bt")) {
			return test_bluetooth();
		} else if (!strcmp(argv[i], "--test-dummy-mod")) {
			return pak_rt_test_module(pak_rt_mod_from_native(get_module_dummy));
		}
	}
	printf("Invalid argument\n");
	return -1;
}
