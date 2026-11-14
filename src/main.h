#pragma once
struct PakModule *pak_create_mod(void);

int pak_rt_test_module(struct PakModule *mod);

struct PakModule *pak_rt_mod_from_native(int (*get)(struct PakModule *mod));

int get_module_dummy(struct PakModule *mod);

int pak_rt_test_module(struct PakModule *mod);

#define to_wrapper(ptr) _Generic((ptr), \
    struct PakBtDevice *             : (struct PakBtDeviceWrapper *)(ptr), \
    const struct PakBtDevice *       : (const struct PakBtDeviceWrapper *)(ptr), \
    struct PakBtAdapter *            : (struct PakBtAdapterWrapper *)(ptr), \
    const struct PakBtAdapter *      : (const struct PakBtAdapterWrapper *)(ptr), \
    struct PakGattService *          : (struct PakGattServiceWrapper *)(ptr), \
    const struct PakGattService *    : (const struct PakGattServiceWrapper *)(ptr), \
    struct PakGattCharacteristic *   : (struct PakGattCharacteristicWrapper *)(ptr), \
    const struct PakGattCharacteristic * : (const struct PakGattCharacteristicWrapper *)(ptr) \
)

#define from_wrapper(ptr) _Generic((ptr), \
    struct PakBtDeviceWrapper *             : (struct PakBtDevice *)(ptr), \
    const struct PakBtDeviceWrapper *       : (const struct PakBtDevice *)(ptr), \
    struct PakBtAdapterWrapper *            : (struct PakBtAdapter *)(ptr), \
    const struct PakBtAdapterWrapper *      : (const struct PakBtAdapter *)(ptr), \
    struct PakGattServiceWrapper *          : (struct PakGattService *)(ptr), \
    const struct PakGattServiceWrapper *    : (const struct PakGattService *)(ptr), \
    struct PakGattCharacteristicWrapper *   : (struct PakGattCharacteristic *)(ptr), \
    const struct PakGattCharacteristicWrapper * : (const struct PakGattCharacteristic *)(ptr) \
)