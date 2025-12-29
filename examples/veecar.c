// Interact with Veecar/Veement dashcams
#include <stdio.h>
#include <stdlib.h>
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

	int n_aps = pak_wifi_get_n_aps(ctx, &adapter);
	struct PakWiFiAp ap;
	for (int i = 0; i < n_aps; i++) {
		if (pak_wifi_get_ap(ctx, &adapter, &ap, i)) return -1;
		if (!strncmp(ap.ssid, "V300", 4)) {
			printf("Found '%s'\n", ap.ssid);
			break;
		}
		pak_wifi_unref_ap(ctx, &adapter, &ap);
	}

	pak_wifi_connect_to_ap(ctx, &adapter, &ap);

	pak_wifi_unref_adapter(ctx, &adapter);

	return 0;
}
