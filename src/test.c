#include <stdio.h>
//#include <stdlib.h>
#include <unistd.h>
#include "wifi.h"
#include "bluetooth.h"

int test_bluetooth(void);

int test_wifi(void) {
	struct PakWiFi *ctx = pak_wifi_get_context();

	struct PakWiFiApList *aps = NULL;
	if (pak_wifi_get_ap_list(ctx, &aps)) return -1;

	for (int i = 0; i < aps->length; i++) {
		printf("'%s'\n", aps->list[i].ssid);
	}

	return 0;
}

int main(void) {
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
