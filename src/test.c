#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "wifi.h"
#include "bluetooth.h"

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
	if (len < 0) return -1;
	struct PakWiFiAdapter adapter;
	for (int i = 0; i < len; i++) {
		if (pak_wifi_get_adapter(ctx, &adapter, i)) return -1;
		printf("adapter: %s\n", adapter.name);
		pak_wifi_unref_adapter(ctx, &adapter);
	}

	struct PakWiFiApList *aps = NULL;
	if (pak_wifi_get_ap_list(ctx, &aps)) return -1;

	for (int i = 0; i < aps->length; i++) {
		printf("'%s'\n", aps->list[i].ssid);
	}

	free(aps);

	struct PakWiFiAp ap;

	pak_wifi_get_connected_ap(ctx, &ap);
	printf("ssid: '%s'\n", ap.ssid);

	return 0;
}

int main(void) {
	return test_wifi();
	return 0;
}
