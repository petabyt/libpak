//#include <stdio.h>
//#include <stdlib.h>
#include <unistd.h>
//#include <string.h>
#include <regex.h>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>
#include "wifi.h"

__attribute__((weak))
int pak_wifi_request_connection(struct PakNet *ctx, struct PakWiFiApFilter *spec, int (*cb)(struct PakNet *ctx, struct PakWiFiAdapter *, void *arg), void *arg) {
	regex_t regex;

	struct PakWiFiAdapter adapter;

	int n_adapters = pak_wifi_get_n_adapters(ctx);
	if (n_adapters <= 0) return -1;
	if (pak_wifi_get_adapter(ctx, &adapter, 0)) return -1;

	if (spec->has_ssid) {
		if (regcomp(&regex, spec->ssid_pattern, 0)) return -1;
	}

	struct PakWiFiAp current_ap;
	if (pak_wifi_get_connected_ap(ctx, &adapter, &current_ap) == 0) {
		if (regexec(&regex, current_ap.ssid, 0, NULL, 0) == 0) {
			pak_wifi_unref_ap(ctx, &adapter, &current_ap);
			cb(ctx, &adapter, arg);
			pak_wifi_unref_adapter(ctx, &adapter);
			return 0;
		}
		pak_wifi_unref_ap(ctx, &adapter, &current_ap);
	}

	int rc = pak_wifi_request_scan(ctx, &adapter);
	if (rc) return -1;

	int n_aps = pak_wifi_get_n_aps(ctx, &adapter);
	struct PakWiFiAp ap;
	for (int i = 0; i < n_aps; i++) {
		if (pak_wifi_get_ap(ctx, &adapter, &ap, i)) return -1;

		rc = regexec(&regex, ap.ssid, 0, NULL, 0);
		if (rc != 0) {
			pak_wifi_unref_ap(ctx, &adapter, &ap);
			continue;
		}
		pak_wifi_connect_to_ap(ctx, &adapter, &ap, spec->password);

		struct PakNetworkHandle handle = {
			.type = PAK_WIFI,
			.adapter = &adapter,
			.u.ap = ap,
		};

		pak_wifi_unref_ap(ctx, &adapter, &ap);
		cb(ctx, &adapter, arg);
	}

	pak_wifi_unref_adapter(ctx, &adapter);

	return 0;
}
