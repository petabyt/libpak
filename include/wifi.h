#pragma once
#include <stdint.h>
#include "pak.h"

struct PakWiFi;
struct PakNetworkHandle;

enum PakWiFiFeature {
	PAK_SUPPORT_GET_AP_LIST = 1,
	PAK_SUPPORT_ANDROID_REQUEST_DIALOG,
	PAK_SUPPORT_CONCURRENT_CONNECTIONS,
	PAK_SUPPORT_2GHZ,
	PAK_SUPPORT_5GHZ,
};

enum PakWiFiBand {
	PAK_WIFI_2GHZ = 1,
	PAK_WIFI_5GHZ,
};

struct PakWiFi *pak_wifi_get_context(void);
void pak_wifi_unref_context(struct PakWiFi *ctx);

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

struct PakWiFiApFilter {
	int has_ssid;
	char ssid_pattern[64];
	int has_bssid;
	char bssid_mask[6];
	char bssid[6];
	int has_password;
	char password[64];
	enum PakWiFiBand band;
	int is_hidden;
};

int pak_wifi_android_network_request_dialog(struct PakWiFi *ctx, struct PakWiFiApFilter *spec, int (*cb)(struct PakWiFi *ctx, struct PakNetworkHandle *), void *arg);
