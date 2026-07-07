#ifndef PAKBLUETOOTH_H
#define PAKBLUETOOTH_H
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
	PAK_BT_EVENT_CONNECTED = 1,
	PAK_BT_EVENT_DISCONNECTED = 2,
	PAK_BT_EVENT_GATT_CHAR_WRITTEN = 3,
	PAK_BT_EVENT_GATT_CHAR_READ = 4,
	PAK_BT_EVENT_GATT_CHAR_CHANGED = 5,
	PAK_BT_EVENT_GATT_DESC_WRITTEN = 6,
	PAK_BT_EVENT_SERVICES_DISCOVERED = 7,
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
};

int pak_bt_get_n_adapters(struct PakBt *ctx);
struct PakBtAdapter *pak_bt_get_adapter(struct PakBt *ctx, int index);
int pak_bt_unref_adapter(struct PakBt *ctx, struct PakBtAdapter *adapter);

struct PakBtDevice {
	struct PakBtDevicePriv *priv;
	_pad_pointer pad_priv;
	int is_classic;
	int is_connected;
	int is_paired;
	char name[64];
	char mac_address[64];
	uint32_t btclass;
};

enum PakDeviceStateFilter {
	PAK_FILTER_BONDED = 1,
	PAK_FILTER_CONNECTED = 2,
};

/// Get number of bluetooth devices that passthrough a filter
/// @param filter See enum PakDeviceStateFilter
int pak_bt_get_n_devices(struct PakBt *ctx, struct PakBtAdapter *adapter, int filter);

/// Get bluetooth devices through a filter
/// @param filter See enum PakDeviceStateFilter
struct PakBtDevice *pak_bt_get_device(struct PakBt *ctx, struct PakBtAdapter *adapter, int index, int filter);

int pak_bt_unref_device(struct PakBt *ctx, struct PakBtDevice *device);

/// @brief Update all fields in device struct to current values
int pak_bt_device_update(struct PakBt *ctx, struct PakBtDevice *dev);

/// Establish connection with a bluetooth device for this context
int pak_bt_device_connect(struct PakBt *ctx, struct PakBtDevice *device);

int pak_bt_device_disconnect(struct PakBt *ctx, struct PakBtDevice *device);

int pak_bt_device_pair(struct PakBt *ctx, struct PakBtDevice *device);

unsigned int pak_bt_get_manufacturer_data(struct PakBt *ctx, struct PakBtDevice *device, int index, uint8_t *buffer, unsigned int max);

struct PakGattService {
	struct PakGattServicePriv *priv;
	_pad_pointer pad_priv;
	char uuid[UUID_STR_LENGTH];
	uint16_t handle;
};

/// Get GATT services within a device
struct PakGattService *pak_bt_get_gatt_service(struct PakBt *ctx, struct PakBtDevice *device, int index);
int pak_bt_unref_gatt_service(struct PakBt *ctx, struct PakGattService *service);

__attribute__((unused))
static struct PakGattService *pak_bt_get_gatt_service_uuid(struct PakBt *ctx, struct PakBtDevice *device, const char *uuid) {
	struct PakGattService *service;
	for (int i = 0; (service = pak_bt_get_gatt_service(ctx, device, i)) != NULL; i++) {
		if (!strcasecmp(service->uuid, uuid)) return 0;
		pak_bt_unref_gatt_service(ctx, service);
	}
	return NULL;
}

struct PakGattCharacteristic {
	struct PakGattCharacteristicPriv *priv;
	_pad_pointer pad_priv;
	int flags;
	char uuid[UUID_STR_LENGTH];
	uint16_t handle;
	uint16_t mtu;
};

/// Get GATT characteristics associated with a GATT service
struct PakGattCharacteristic *pak_bt_get_gatt_characteristic(struct PakBt *ctx, struct PakGattService *service, int index);
int pak_bt_unref_gatt_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *chr);

__attribute__((unused))
static struct PakGattCharacteristic *pak_bt_get_gatt_characteristic_uuid(struct PakBt *ctx, struct PakGattService *service, const char *uuid) {
	struct PakGattCharacteristic *characteristic;
	for (int i = 0; (characteristic = pak_bt_get_gatt_characteristic(ctx, service, i)) == 0; i++) {
		if (!strcasecmp(characteristic->uuid, uuid)) return 0;
		pak_bt_unref_gatt_characteristic(ctx, characteristic);
	}
	return NULL;
}

/// Request to read characteristic value. Result will be fired in the callback
int pak_bt_read_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *characteristic, int blocking);

/// Request to write data to a characteristic. Write acknowledgement will be fired in the callback
int pak_bt_write_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *characteristic, const uint8_t *data, unsigned int length, int blocking);

/// @brief Read cached characteristic value
/// @returns Number of bytes read, 0 if error
unsigned int pak_bt_read_characteristic_cached_value(struct PakBt *ctx, struct PakGattCharacteristic *characteristic, uint8_t *buffer, unsigned int max);

/// @brief Block thread and watch characteristic changes for x ms or until event occurs
int pak_bt_watch_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *characteristic, unsigned int ms);

/// @brief Set whether ctx is watching the characteristic
int pak_bt_set_watching_characteristic(struct PakBt *ctx, struct PakGattCharacteristic *characteristic, int v);

/// @brief Helper function to set Client Characteristic Configuration Descriptor value
int pak_bt_set_cccd(struct PakBt *ctx, struct PakGattCharacteristic *characteristic, int v);

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
struct PakBtSocket *pak_bt_connect_to_service_channel(struct PakBt *ctx, struct PakBtDevice *dev, const char *uuid);
/// write()
int pak_bt_write(struct PakBtSocket *conn, const void *data, unsigned int length);
/// read()
int pak_bt_read(struct PakBtSocket *conn, void *data, unsigned int length);
/// close()
int pak_bt_close_socket(struct PakBtSocket *conn);

typedef int pak_bt_listen_device(struct PakBt *ctx, enum PakBtEvent evtype, struct PakBtDevice *dev, struct PakGattCharacteristic *chr, void *arg);

/// @brief Set event callback for a device
int pak_bt_set_device_callback(struct PakBt *ctx, struct PakBtDevice *device, pak_bt_listen_device *cb, void *cb_arg);

#endif
