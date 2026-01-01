#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>
#include "wifi.h"
#include "bluetooth.h"

JSModuleDef *js_init_module_socket(JSContext *ctx, const char *module_name);
JSModuleDef *js_init_module_wifi(JSContext *ctx, const char *module_name);

int run_quickjs(const char *filename) {
	JSRuntime *rt = JS_NewRuntime();

	JSContext *ctx = JS_NewContext(rt);
	js_std_add_helpers(ctx, 0, NULL);

	JS_AddModuleExport(ctx, js_init_module_wifi(ctx, "pak:wifi"), "WiFi");
	js_init_module_socket(ctx, "c:socket");
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

	printf("Return value: %d\n", JS_VALUE_GET_TAG(val));

	JS_FreeValue(ctx, val);

	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);

	return 0;
}


int test_bluetooth(void) {
	struct PakBtAdapter adapter;
	struct PakBt *ctx = pak_bt_get_context();
	int n = pak_bt_get_n_adapters(ctx);
	if (n < 0) return -1;
	for (int i = 0; i < n; i++) {
		if (pak_bt_get_adapter(ctx, &adapter, i)) return -1;
		printf("Bluetooth adapter: '%s'\n", adapter.name);
		break;
	}

	if (!pak_bt_is_enabled(ctx)) {
		printf("Bluetooth is not enabled\n");
		return 0;
	}

	pak_bt_unref_adapter(ctx, &adapter);

	return 0;
}

int test_wifi(void) {
	struct PakWiFi *ctx = pak_wifi_get_context();

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

	pak_wifi_unref_adapter(ctx, &adapter);

	return 0;
}

int main(int argc, char **argv) {
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--js")) {
			return run_quickjs(argv[i + 1]);
		} else if (!strcmp(argv[i], "--test")) {
			int rc = test_wifi();
			rc |= test_bluetooth();
			return rc;
		}
	}
	printf("Invalid argument\n");
	return -1;
}
