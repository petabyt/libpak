#pragma once
#include <stdint.h>
#include "pak.h"

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
	uint8_t (*uuids)[16];
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
int pak_bt_connect_to_service_channel(struct PakBt *ctx, struct PakBtDevice *dev, uint8_t uuid[16], struct PakBtSocket **conn);

int pak_bt_write(struct PakBtSocket *conn, const void *data, unsigned int length);
int pak_bt_read(struct PakBtSocket *conn, void *data, unsigned int length);
int pak_bt_close_socket(struct PakBtSocket *conn);

typedef int pak_bt_listen(struct PakBt *ctx, enum PakBtEvent evtype, struct PakBtDevice *adv);

int pak_bt_listen_advertisements(struct PakBt *ctx, struct PakBtAdapter *adapter, pak_bt_listen *cb, void *cb_arg);

static int pak_str_to_uuid128(const char *str, uint8_t copy[16]) {
	int32_t v[16];
	if (sscanf(str, "%2x%2x%2x%2x-%2x%2x-%2x%2x-%2x%2x-%2x%2x%2x%2x%2x%2x",
			&v[0], &v[1], &v[2], &v[3],
			&v[4], &v[5],
			&v[6], &v[7],
			&v[8], &v[9],
			&v[10], &v[11], &v[12], &v[13], &v[14], &v[15]) != 16)
		return -1;
	for (int i = 0; i < 16; i++) {
		copy[i] = v[i];
	}
	return 0;
}

static void pak_uuid128_to_str(const uint8_t in[16], char str[37]) {
	sprintf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		in[0], in[1], in[2], in[3],
		in[4], in[5],
		in[6], in[7],
		in[8], in[9],
		in[10], in[11], in[12], in[13], in[14], in[15]);
}
