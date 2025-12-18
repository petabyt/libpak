#pragma once
#include <stdint.h>

struct PakBt;

struct PakBt *pak_bt_get_context(void);

int pak_is_bluetooth_enabled(struct PakBt *ctx);

enum PakEvent {
	PAK_EVENT_FOO,
};

struct PakBtAdapter {
	char address[64];
	char name[64];
	int powered;
	int class;
	void *priv;
};

struct PakBtAdapterList {
	int length;
	struct PakBtAdapter list[];
};

/// @brief Get a list of bluetooth adapters that can be used
int pak_bt_get_adapters(struct PakBt *ctx, struct PakBtAdapterList **list_arg);

struct PakBtAdvertisement {
	char name[64];
	char mac_address[64];
	uint8_t mfg_data[64];
	int adv_interval;
};

struct PakBtAdvertisementList {
	int length;
	struct PakBtAdvertisement list[];
};

/// @brief Get a list of current advertisements on an adapter. Does not scan for new devices.
int pak_bt_get_advertisements(struct PakBt *ctx, struct PakBtAdapter *adapter, struct PakBtAdvertisementList **list_arg);

typedef int pak_bt_listen(struct PakBt *ctx, enum PakEvent, struct PakBtAdvertisement *adv);

int pak_bt_listen_advertisements(struct PakBt *ctx, struct PakBtAdapter *adapter, pak_bt_listen *cb, void *cb_arg);

struct PakBtConnection;
int pak_btc_connect_to_service_channel(struct PakBt *ctx, struct PakBtAdvertisement *adv, struct PakBtConnection **conn);
