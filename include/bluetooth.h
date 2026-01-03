#pragma once
#include <stdint.h>
#include "pak.h"

/// Bluetooth context
struct PakBt;

struct PakBt *pak_bt_get_context(void);

/// Checks if system-wide bluetooth is enabled
int pak_bt_is_enabled(struct PakBt *ctx);

enum PakBtFeature {
	PAK_SUPPORT_LE_ADVERTISING = 1,
	PAK_SUPPORT_LE_AUDIO,
};
int pak_bt_is_supported(enum PakBtFeature feat);

struct PakBt *pak_bt_get_context(void);
void pak_bt_unref_context(struct PakBt *ctx);

enum PakBtEvent {
	PAK_EVENT_FOO = 1,
};

enum PakBtSocketOption {
	PAK_RFCOMM_SECURE = 1 << 0,
	PAK_RFCOMM_INSECURE = 1 << 1,
};

struct PakBtAdapter {
	char address[64];
	char name[64];
	int powered;
	int class_;
	struct PakBtAdapterPriv *priv;
	_pad_pointer pad_priv;
};

int pak_bt_get_n_adapters(struct PakBt *ctx);
int pak_bt_get_adapter(struct PakBt *ctx, struct PakBtAdapter *adapter, int index);
int pak_bt_unref_adapter(struct PakBt *ctx, struct PakBtAdapter *adapter);

struct PakBtDevice {
	int is_classic;
	int is_connected;
	char name[64];
	char mac_address[64];
	uint32_t btclass;
	uint8_t mfg_data[64];
	struct PakBtDevicePriv *priv;
	_pad_pointer pad_priv;
};

int pak_bt_get_paired_device(struct PakBt *ctx, struct PakBtAdapter *adapter, struct PakBtDevice *device, int index);

/// Bluetooth RFCOMM socket
struct PakBtSocket;

/// Setup RFCOMM connection to a UUID via SDP lookup
int pak_bt_connect_to_service_channel(struct PakBt *ctx, struct PakBtDevice *dev, uint8_t uuid[16], struct PakBtSocket **conn);

/// @brief Get a list of current cached advertisements on an adapter. Does not perform a scan.
//int pak_bt_get_advertisements(struct PakBt *ctx, struct PakBtAdapter *adapter, struct PakBtAdvertisementList **list_arg);

typedef int pak_bt_listen(struct PakBt *ctx, enum PakBtEvent evtype, struct PakBtDevice *adv);

int pak_bt_listen_advertisements(struct PakBt *ctx, struct PakBtAdapter *adapter, pak_bt_listen *cb, void *cb_arg);
