#pragma once
#include <stdint.h>
#include "pak.h"

struct PakNet;

enum PakWiFiFeature {
	// Get arbitrary list of access points
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

enum PakAdapterType {
	PAK_WIFI = 1,
	PAK_ETHERNET,
	PAK_CELLULAR,
};

struct PakNet *pak_net_get_context(void);
void pak_net_unref_context(struct PakNet *ctx);

/// @brief Returns true if system-wide WiFi switch is enabled
int pak_wifi_is_enabled(struct PakNet *ctx);

struct PakWiFiAdapter {
	struct PakWiFiAdapterPriv *priv;
	_pad_pointer pad_priv;
	char name[32];
	int is_active;
};

int pak_wifi_get_n_adapters(struct PakNet *ctx);
int pak_wifi_get_adapter(struct PakNet *ctx, struct PakWiFiAdapter *adapter, int index);
int pak_wifi_unref_adapter(struct PakNet *ctx, struct PakWiFiAdapter *adapter);

/// Bind a socket file descriptor to a network adapter
int pak_wifi_bind_socket_to_adapter(struct PakNet *ctx, struct PakWiFiAdapter *adapter, int fd);

/// Request the adapter to perform a scan (roam)
int pak_wifi_request_scan(struct PakNet *ctx, struct PakWiFiAdapter *adapter);

struct PakWiFiAp {
	struct PakWiFiApPriv *priv;
	_pad_pointer pad_priv;
	char ssid[33];
	char bssid[6];
	enum PakWiFiBand band;
};

int pak_wifi_get_n_aps(struct PakNet *ctx, struct PakWiFiAdapter *adapter);
int pak_wifi_get_ap(struct PakNet *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap, int index);
int pak_wifi_unref_ap(struct PakNet *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap);

/// @returns -1 if adapter is currently not connected to access point
int pak_wifi_get_connected_ap(struct PakNet *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap);

/// Request to connect to access point
int pak_wifi_connect_to_ap(struct PakNet *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap, const char *password);

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

/// Tries to open a connection with the specified PakWiFiApFilter
/// Will automatically select an adapter, prefers one that is not currently connected to anything
int pak_wifi_request_connection(struct PakNet *ctx, struct PakWiFiApFilter *spec, int (*cb)(struct PakNet *ctx, struct PakWiFiAdapter *, void *arg), void *arg);

struct PakNetworkHandle {
	struct PakWiFiAdapter *adapter;
	enum PakAdapterType type;
	union {
		struct PakWiFiAp ap;
	}u;
};
int pak_net_bind_handle_to_socket(struct PakNet *ctx, struct PakNetworkHandle *handle, int fd);
