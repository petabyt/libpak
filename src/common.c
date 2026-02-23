// Implements higher level functionality found on Android (companion APIs and such)
//#include <stdio.h>
//#include <stdlib.h>
#include <unistd.h>
//#include <string.h>
#include <regex.h>
#include <stdarg.h>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>
#include "wifi.h"

__attribute__((weak))
void pak_error(const char *fmt, ...) {
	printf("ERR: ");
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

__attribute__((weak))
void pak_abort(const char *fmt, ...) {
	printf("ERR: ");
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	fflush(stdout);
	abort();
}

__attribute__((weak))
int pak_wifi_request_connection(struct PakNet *ctx, struct PakWiFiApFilter *spec, int (*cb)(struct PakNet *ctx, struct PakWiFiAdapter *, void *arg), void *arg) {
	regex_t regex;

	struct PakWiFiAdapter adapter;

	int n_adapters = pak_wifi_get_n_adapters(ctx);
	if (n_adapters <= 0) return -1;
	if (pak_wifi_get_adapter(ctx, &adapter, 0)) {
		pak_error("pak_wifi_get_adapter\n");
		return -1;
	}

	if (spec->has_ssid) {
		if (regcomp(&regex, spec->ssid_pattern, 0)) {
			pak_error("regcomp\n");
			return -1;
		}
	}

	struct PakWiFiAp current_ap;
	if (pak_wifi_get_connected_ap(ctx, &adapter, &current_ap) == 0) {
		if (regexec(&regex, current_ap.ssid, 0, NULL, 0) == 0) {
			pak_wifi_unref_ap(ctx, &adapter, &current_ap);
			cb(ctx, &adapter, arg);
			//pak_wifi_unref_adapter(ctx, &adapter);
			return 0;
		}
		pak_wifi_unref_ap(ctx, &adapter, &current_ap);
	}

	int rc = pak_wifi_request_scan(ctx, &adapter);
	if (rc) {
		pak_error("pak_wifi_request_scan\n");
		return -1;
	}

	int n_aps = pak_wifi_get_n_aps(ctx, &adapter);
	struct PakWiFiAp ap;
	for (int i = 0; i < n_aps; i++) {
		if (pak_wifi_get_ap(ctx, &adapter, &ap, i)) {
			pak_error("pak_wifi_get_ap\n");
			return -1;
		}

		rc = regexec(&regex, ap.ssid, 0, NULL, 0);
		if (rc != 0) {
			pak_wifi_unref_ap(ctx, &adapter, &ap);
			continue;
		}
		if (pak_wifi_connect_to_ap(ctx, &adapter, &ap, spec->password)) {
			return PAK_NOT_CONNECTED;
		}

		pak_wifi_unref_ap(ctx, &adapter, &ap);
		cb(ctx, &adapter, arg);
		return 0;
	}

	pak_wifi_unref_adapter(ctx, &adapter);

	return PAK_NOT_CONNECTED;
}
