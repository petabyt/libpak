#pragma once
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pak.h"

// Max size of RFC compliant cstring UUID
#define UUID_STR_LENGTH 37

/// Bluetooth context
struct PakBt;

struct PakBt *pak_bt_get_context(void);
void pak_bt_unref_context(struct PakBt *ctx);

/// Checks if system-wide bluetooth is enabled
int pak_bt_is_enabled(struct PakBt *ctx);

enum PakBtEvent {
	PAK_BT_EVENT_CONNECTION_STATE_CHANGED = 1,
	PAK_BT_EVENT_CONNECTED,
	PAK_BT_EVENT_DISCONNECTED,
	PAK_BT_EVENT_GATT_UUID_WRITTEN,
	PAK_BT_EVENT_GATT_UUID_READ,
	PAK_BT_EVENT_DEVICE_PAIRED,
	PAK_BT_EVENT_DEVICE_UNPAIRED,
};

enum PakBtFeature {
	/// Support BLE
	PAK_SUPPORT_BLE = 1,
	PAK_SUPPORT_BLE_ADVERTISING,
	PAK_SUPPORT_BLE_AUDIO,
	/// Support bluetooth classic
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
	char (*uuids)[UUID_STR_LENGTH];
};

struct PakBtDevice {
	struct PakBtDevicePriv *priv;
	_pad_pointer pad_priv;
	int is_classic;
	int is_connected;
	int is_paired;
	char name[64];
	char mac_address[64];
	uint32_t btclass;
	uint8_t mfg_data[0xff];
	struct PakUuidList uuids;
};

/// Get devices currently paired with the system
int pak_bt_get_paired_device(struct PakBt *ctx, struct PakBtAdapter *adapter, struct PakBtDevice *device, int index);

/// Get devices that are currently not paired but saved to the system's bluetooth manager
int pak_bt_get_saved_device(struct PakBt *ctx, struct PakBtAdapter *adapter, struct PakBtDevice *device, int index);

int pak_bt_unref_device(struct PakBt *ctx, struct PakBtDevice *device);

/// Use generic API to read battery level
int pak_bt_get_device_battery(struct PakBt *ctx, struct PakBtDevice *device, int *percent);

/// Establish connection with a bluetooth device for this context
int pak_bt_device_connect(struct PakBt *ctx, struct PakBtDevice *device);

int pak_bt_device_disconnect(struct PakBt *ctx, struct PakBtDevice *device);

int pak_bt_device_pair(struct PakBt *ctx, struct PakBtDevice *device);

struct PakGattService {
	struct PakGattServicePriv *priv;
	_pad_pointer pad_priv;
	char uuid[UUID_STR_LENGTH];
	uint16_t handle;
};

/// Get GATT services within a device
int pak_bt_get_gatt_service(struct PakBt *ctx, struct PakBtDevice *device, struct PakGattService *service, int index);
int pak_bt_unref_gatt_service(struct PakBt *ctx, struct PakGattService *service);

struct PakGattCharacteristic {
	struct PakGattCharacteristicPriv *priv;
	_pad_pointer pad_priv;
	int flags;
	char uuid[UUID_STR_LENGTH];
	uint16_t handle;
	uint16_t mtu;
};

/// Get GATT characteristics associated with a GATT service
int pak_bt_get_gatt_characteristic(struct PakBt *ctx, struct PakGattService *service, struct PakGattCharacteristic *characteristic, int index);
int pak_bt_unref_gatt_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *chr);

/// Request to read characteristic value. Result will be fired in the callback
int pak_bt_read_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *characteristic, int blocking);

/// Request to write data to a characteristic. Write acknowledgement will be fired in the callback
int pak_bt_write_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *characteristic, uint8_t *data, unsigned int length, int blocking);

struct PakGattDescriptor {
	struct PakGattDescriptorPriv *priv;
	_pad_pointer pad_priv;
	char uuid[UUID_STR_LENGTH];
	uint16_t handle;
};

/// Bluetooth Classic RFCOMM socket
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
