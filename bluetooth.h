#pragma once
#include <stdint.h>

struct PakBt;

struct PakBt *pak_bt_get_context(void);

int pak_is_bluetooth_enabled(struct PakBt *ctx);

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

int pak_bt_get_advertisements(struct PakBt *ctx, struct PakBtAdapter *adapter, struct PakBtAdvertisementList **list_arg);
