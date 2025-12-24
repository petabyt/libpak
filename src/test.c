#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>
#include "wifi.h"
#include "bluetooth.h"

JSModuleDef *js_init_module_wifi(JSContext *ctx, const char *module_name);

int run_quickjs(const char *filename) {
	JSRuntime *rt = JS_NewRuntime();

	JSContext *ctx = JS_NewContext(rt);
	js_std_add_helpers(ctx, 0, NULL);

	JS_AddModuleExport(ctx, js_init_module_wifi(ctx, "pak:wifi"), "WiFi");
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
		js_std_dump_error(ctx);
		const char *str = JS_ToCString(ctx, val);
		printf("JS error: %s\n", str);
		JS_FreeCString(ctx, str);
		return -1;
	}

	JS_FreeValue(ctx, val);

	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);

	return 0;
}


int test_bluetooth(void) {
	struct PakBtAdapterList *adapters;
	pak_bt_get_adapters(NULL, &adapters);

	for (int i = 0; i < adapters->length; i++) {
		printf("Bluetooth adapter: '%s'\n", adapters->list[i].name);
		//printf("'%s'\n", adapters->list[i].address);
	}

	struct PakBtAdvertisementList *advs;
	pak_bt_get_advertisements(NULL, NULL, &advs);

	for (int i = 0; i < advs->length; i++) {
		printf("Advertisement: '%s'\n", advs->list[i].name);
		printf("mac address: '%s'\n", advs->list[i].mac_address);
	}

	if (!pak_bt_is_enabled(NULL)) {
		printf("Bluetooth is not enabled\n");
		return 0;
	}

	//struct PakBtConnection *conn;
	//pak_btc_connect_to_service_channel(NULL, NULL, &conn);

	return 0;
}

int test_wifi(void) {
	struct PakWiFi *ctx = pak_wifi_get_context();

	int len = pak_wifi_get_n_adapters(ctx);
	if (len <= 0) return -1;
	struct PakWiFiAdapter adapter;
	if (pak_wifi_get_adapter(ctx, &adapter, 0)) return -1;
	printf("adapter: %s\n", adapter.name);

	struct PakWiFiApList *aps = NULL;
	if (pak_wifi_get_ap_list(ctx, &aps)) return -1;

	for (int i = 0; i < aps->length; i++) {
		printf("'%s'\n", aps->list[i].ssid);
	}

	free(aps);

	struct PakWiFiAp ap;

	pak_wifi_get_connected_ap(ctx, &adapter, &ap);
	printf("Connected to '%s'\n", ap.ssid);

	pak_wifi_connect_to_ap(ctx, &adapter, &ap);

	pak_wifi_unref_adapter(ctx, &adapter);

	return 0;
}

int main(void) {
	return test_wifi();
	//return run_quickjs("x.js");
	return 0;
}
