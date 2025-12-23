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

struct PakWiFiAdapter {
	char name[32];
	int is_active;
	uint64_t priv;
};

struct PakWiFiAdapterList {
	int length;
	struct PakWiFiAdapter list[];
};

int pak_wifi_get_n_adapters(struct PakWiFi *ctx);
int pak_wifi_get_adapter(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, int index);
int pak_wifi_unref_adapter(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter);

int pak_wifi_get_adapter_list(struct PakWiFi *ctx, struct PakWiFiAdapterList **ap_list);
int pak_wifi_get_default_adapter(struct PakWiFi *ctx, struct PakWiFiAdapter *ap);
int pak_wifi_free_adapter(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter_arg);
int pak_wifi_free_adapter_list(struct PakWiFi *ctx, struct PakWiFiAdapterList *list_arg);

struct PakWiFiAp {
	char ssid[33];
	char bssid[6];
	uint64_t priv;
};

struct PakWiFiApList {
	int length;
	struct PakWiFiAp list[];
};

int pak_wifi_get_ap_list(struct PakWiFi *ctx, struct PakWiFiApList **ap_list);

int pak_wifi_get_connected_ap(struct PakWiFi *ctx, struct PakWiFiAp *ap);

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
