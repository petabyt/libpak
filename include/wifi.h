#pragma once
#include <stdint.h>
#include "pak.h"

// TODO: Rename to Network instead of WiFi (to support binding to ethernet/cellular adapter)

struct PakWiFi;

enum PakWiFiFeature {
	// Get arbritrary list of access points
	PAK_SUPPORT_GETTING_AP_LIST = 1,
	/// OS WiFi pairing prompt supported
	PAK_SUPPORT_ANDROID_REQUEST_DIALOG,
	/// WiFi STA/STA concurrency
	PAK_SUPPORT_CONCURRENT_CONNECTIONS,
	/// Is 2.4ghz band supported
	PAK_SUPPORT_2GHZ,
	/// Is 5ghz band supported
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
	struct PakWiFiAdapterPriv *priv;
	_pad_pointer pad_priv;
	char name[32];
	int is_active;
};

int pak_wifi_get_n_adapters(struct PakWiFi *ctx);
int pak_wifi_get_adapter(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, int index);
int pak_wifi_unref_adapter(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter);

/// Bind a socket file descriptor to a network adapter
int pak_wifi_bind_socket_to_adapter(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, int fd);

/// Request the adapter to perform a scan (roam)
int pak_wifi_request_scan(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter);

struct PakWiFiAp {
	struct PakWiFiApPriv *priv;
	_pad_pointer pad_priv;
	char ssid[33];
	char bssid[6];
	enum PakWiFiBand band;
};

struct PakNetworkHandle {
	struct PakWiFiAdapter adapter;
	union {
		struct PakWiFiAp ap;
	}u;
};

int pak_wifi_get_n_aps(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter);
int pak_wifi_get_ap(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap, int index);
int pak_wifi_unref_ap(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap);

/// @returns -1 if adapter is currently not connected to access point
int pak_wifi_get_connected_ap(struct PakWiFi *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap);

// 
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
