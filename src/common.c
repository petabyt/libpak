// Implements higher level functionality found on Android (companion APIs and such)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
//#include <string.h>
#include <regex.h>
#include <stdarg.h>
#include "wifi.h"
#include "runtime.h"

__attribute__((weak))
void pak_global_log(const char *fmt, ...) {
	
}

__attribute__((weak))
void pak_error(const char *fmt, ...) {
	printf("ERR: ");
	fflush(stdout);
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

__attribute__((weak))
void pak_abort(const char *fmt, ...) {
	printf("ERR: ");
	fflush(stdout);
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
			// Callback should run pak_wifi_unref_adapter on adapter
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
			return PAK_ERR_NO_CONNECTION;
		}

		pak_wifi_unref_ap(ctx, &adapter, &ap);
		cb(ctx, &adapter, arg);
		return 0;
	}

	pak_wifi_unref_adapter(ctx, &adapter);

	return PAK_ERR_NO_CONNECTION;
}

int get_pak_timestamp(struct PakTimestamp *ts) {
	struct timespec ts_spec;

	if (clock_gettime(CLOCK_REALTIME, &ts_spec) != 0) {
		return -1;
	}

	struct tm time_info;
	if (localtime_r(&ts_spec.tv_sec, &time_info) == NULL) {
		return -1;
	}

	ts->year        = (unsigned int)(time_info.tm_year + 1900);
	ts->month       = (unsigned int)(time_info.tm_mon + 1);
	ts->day         = (unsigned int)time_info.tm_mday;
	ts->hour        = (unsigned int)time_info.tm_hour;
	ts->minute      = (unsigned int)time_info.tm_min;
	ts->second      = (unsigned int)time_info.tm_sec;
	ts->centisecond = (unsigned int)(ts_spec.tv_nsec / 10000000);

	return 0;
}
