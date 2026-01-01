#pragma once
#include <stdint.h>
#include "pak.h"

// TODO: Rename to Network instead of WiFi (to support binding to ethernet/cellular adapter)

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
	struct PakWiFiAdapterPriv *priv;
	_pad_pointer pad_priv;
};

int pak_wifi_get_n_adapters(struct PakWiFi *ctx);
int pak_wifi_get_adapter(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, int index);
int pak_wifi_unref_adapter(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter);

/// Bind a socket to a network adapter
int pak_wifi_bind_socket_to_adapter(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, int fd);

struct PakWiFiAp {
	char ssid[33];
	char bssid[6];
	enum PakWiFiBand band;
	struct PakWiFiApPriv *priv;
	_pad_pointer pad_priv;
};

int pak_wifi_get_n_aps(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter);
int pak_wifi_get_ap(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap, int index);
int pak_wifi_unref_ap(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap);

int pak_wifi_get_connected_ap(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap);

int pak_wifi_connect_to_ap(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap, const char *password);

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

// TODO: Companion APIs
