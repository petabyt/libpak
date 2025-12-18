#pragma once
#include <stdint.h>

struct PakWiFi;

enum PakWiFiErrorCode {
	PAK_WIFI_PERMISSION_DENIED = -1,
};

struct PakWiFi *pak_wifi_get_context(void);

int pak_wifi_is_enabled(struct PakWiFi *ctx);

struct PakWiFiDevice {
	char name[32];
	int is_active;
	void *priv;
};

struct PakWiFiDeviceList {
	int length;
	struct PakWiFiDevice list[];
};

int pak_wifi_get_device_list(struct PakWiFi *ctx, struct PakWiFiDeviceList **ap_list);

struct PakWiFiAp {
	char ssid[33];
	char bssid[6];
	void *priv;
};

struct PakWiFiApList {
	int length;
	struct PakWiFiAp list[];
};

int pak_wifi_get_ap_list(struct PakWiFi *ctx, struct PakWiFiApList **ap_list);
