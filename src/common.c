#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>
#include "wifi.h"

__attribute__((weak))
int pak_wifi_request_connection(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiApFilter *spec, int (*cb)(struct PakWiFi *ctx, struct PakNetworkHandle *), void *arg) {
	regex_t regex;

	if (spec->has_ssid) {
		if (regcomp(&regex, spec->ssid_pattern, 0)) return -1;
	}
	
	int rc = pak_wifi_request_scan(ctx, adapter);
	if (rc) return -1;

	int n_aps = pak_wifi_get_n_aps(ctx, adapter);
	struct PakWiFiAp ap;
	for (int i = 0; i < n_aps; i++) {
		if (pak_wifi_get_ap(ctx, adapter, &ap, i)) return -1;
		int is_match = 0;

		rc = regexec(&regex, ap.ssid, 0, NULL, 0);
		if (rc != 0) {
			pak_wifi_unref_ap(ctx, adapter, &ap);
			continue;
		}
		pak_wifi_connect_to_ap(ctx, adapter, &ap, "12345678");
		pak_wifi_unref_ap(ctx, adapter, &ap);
		cb(ctx, );
	}

	pak_wifi_unref_adapter(ctx, adapter);

	return 0;
}
