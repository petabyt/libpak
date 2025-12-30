// Interact with Veecar/Veement dashcams
#include <stdio.h>
//#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <wifi.h>
#include <bluetooth.h>

int main(void) {
	struct PakWiFi *ctx = pak_wifi_get_context();

	int len = pak_wifi_get_n_adapters(ctx);
	if (len <= 0) return -1;
	struct PakWiFiAdapter adapter;
	if (pak_wifi_get_adapter(ctx, &adapter, 0)) return -1;
	printf("adapter: %s\n", adapter.name);

	int found_ssid = 0;
	
	int n_aps = pak_wifi_get_n_aps(ctx, &adapter);
	struct PakWiFiAp ap;
	for (int i = 0; i < n_aps; i++) {
		if (pak_wifi_get_ap(ctx, &adapter, &ap, i)) return -1;
		printf("%s\n", ap.ssid);
		if (!strncmp(ap.ssid, "V300", 4)) {
			found_ssid = 1;
			break;
		}
		pak_wifi_unref_ap(ctx, &adapter, &ap);
	}

	if (found_ssid) {
		pak_wifi_connect_to_ap(ctx, &adapter, &ap, "12345678");
	}

	pak_wifi_unref_adapter(ctx, &adapter);

	return 0;
}
