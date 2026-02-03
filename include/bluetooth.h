#pragma once
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pak.h"

#define UUID_STR_LENGTH 37

/// Bluetooth context
struct PakBt;

struct PakBt *pak_bt_get_context(void);

/// Checks if system-wide bluetooth is enabled
int pak_bt_is_enabled(struct PakBt *ctx);

struct PakBt *pak_bt_get_context(void);
void pak_bt_unref_context(struct PakBt *ctx);

enum PakBtEvent {
	PAK_EVENT_LE_ADVERTISEMENT = 1,
	PAK_EVENT_GATT_UUID_CHANGED,
	PAK_EVENT_DEVICE_PAIRED,
	PAK_EVENT_DEVICE_UNPAIRED,
};

enum PakBtFeature {
	PAK_SUPPORT_LE = 1,
	PAK_SUPPORT_LE_ADVERTISING,
	PAK_SUPPORT_LE_AUDIO,
	PAK_SUPPORT_BTC,
	PAK_SUPPORT_PERIPHERAL_MODE,
};
int pak_bt_is_supported(struct PakBt *ctx, enum PakBtFeature feat);

struct PakBtAdapter {
	struct PakBtAdapterPriv *priv;
	_pad_pointer pad_priv;
	char address[64];
	char name[64];
	int powered;
	int class_;
};

int pak_bt_get_n_adapters(struct PakBt *ctx);
int pak_bt_get_adapter(struct PakBt *ctx, struct PakBtAdapter *adapter, int index);
int pak_bt_unref_adapter(struct PakBt *ctx, struct PakBtAdapter *adapter);

struct PakUuidList {
	unsigned int length;
	char (*uuids)[37];
};

struct PakBtDevice {
	struct PakBtDevicePriv *priv;
	_pad_pointer pad_priv;
	int is_classic;
	int is_connected;
	char name[64];
	char mac_address[64];
	uint32_t btclass;
	uint8_t mfg_data[0xff];
	struct PakUuidList uuids;
};

int pak_bt_get_paired_device(struct PakBt *ctx, struct PakBtAdapter *adapter, struct PakBtDevice *device, int index);

int pak_bt_unref_device(struct PakBt *ctx, struct PakBtDevice *device);

int pak_bt_get_device_battery(struct PakBt *ctx, struct PakBtDevice *device, int *percent);

/// Bluetooth RFCOMM socket
struct PakBtSocket;

enum PakBtSocketOption {
	PAK_RFCOMM_SECURE = 1 << 0,
	PAK_RFCOMM_INSECURE = 1 << 1,
};

/// Setup RFCOMM connection to a UUID via SDP lookup
int pak_bt_connect_to_service_channel(struct PakBt *ctx, struct PakBtDevice *dev, const char *uuid, struct PakBtSocket **conn);
/// write()
int pak_bt_write(struct PakBtSocket *conn, const void *data, unsigned int length);
/// read()
int pak_bt_read(struct PakBtSocket *conn, void *data, unsigned int length);
/// close()
int pak_bt_close_socket(struct PakBtSocket *conn);

typedef int pak_bt_listen_adv(struct PakBt *ctx, enum PakBtEvent evtype, struct PakBtDevice *adv, void *arg);
typedef int pak_bt_listen_gatt(struct PakBt *ctx, enum PakBtEvent evtype, const char *uuid);

int pak_bt_listen_advertisements(struct PakBt *ctx, struct PakBtAdapter *adapter, pak_bt_listen_adv *cb, void *cb_arg);
