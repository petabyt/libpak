/**
 * @file libusb_native.c
 *
 * WASM wrapper for the host libusb-1.0 library.
 *
 * Design rules:
 * - No host pointer is ever stored in guest memory. libusb contexts, devices, device handles, device lists and config
 *   descriptors live in a per module instance pointer_table (see pointer_table.h), stored in the module instance context
 *   (wasm_runtime_create_context_key() and friends, the same mechanism libc-wasi uses for its context). The guest only
 *   sees integer handles typed as opaque pointers in the guest header.
 * - Structs returned to the guest (USB descriptors, device lists) are deep copied into guest memory allocated with
 *   wasm_runtime_module_malloc(), serialized field by field with a hardcoded wasm32 ABI (little endian, 4 byte pointers),
 *   because the host ABI (64 bit pointers) differs from the wasm32 ABI.
 * - Class specific "extra" descriptors are currently nulled out (extra = NULL, extra_length = 0).
 * - Transfers are blocking (libusb sync API); data crosses the boundary through a host staging buffer so memory growth of
 *   the instance can never invalidate a native pointer held across the call.
 * - The instance context destructor runs when the main instance is deinstantiated, after its linear memory (and app heap)
 *   has already been torn down, so the destroy callbacks take a "teardown" argument and skip guest memory frees in that
 *   case (the blobs die with the linear memory anyway). Payloads remember their creating instance so that a normal
 *   (non-teardown) removal always frees the guest blob from the right heap.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <limits.h>
#include "../../toolchain/include/libusb.h"
#include <runtime.h>

#include "wasm_export.h"
// #include "bh_log.h"
#include "pointer_table.h"

#ifndef WASM_ENABLE_MODULE_INST_CONTEXT
#define WASM_ENABLE_MODULE_INST_CONTEXT 0
#endif

#if WASM_ENABLE_MODULE_INST_CONTEXT == 0
#warning "the libusb wrapper requires WASM_ENABLE_MODULE_INST_CONTEXT (WAMR_BUILD_MODULE_INST_CONTEXT=1)"
#endif

// wasm32 ABI layout of the structures that are serialized (hardcoded offsets; verified against the guest header
// through _Static_asserts in wasm/include/libusb.h)
#define W32_SIZEOF_DEVICE_DESCRIPTOR 18
#define W32_SIZEOF_ENDPOINT_DESCRIPTOR 20
#define W32_SIZEOF_INTERFACE_DESCRIPTOR 24
#define W32_SIZEOF_INTERFACE 8
#define W32_SIZEOF_CONFIG_DESCRIPTOR 24

#define W32_CFG_OFF_interface 12
#define W32_CFG_OFF_extra 16
#define W32_CFG_OFF_extra_length 20

#define W32_IFACE_OFF_altsetting 0
#define W32_IFACE_OFF_num_altsetting 4

#define W32_IFD_OFF_endpoint 12
#define W32_IFD_OFF_extra 16
#define W32_IFD_OFF_extra_length 20

#define W32_EPD_OFF_extra 12
#define W32_EPD_OFF_extra_length 16

enum {
	T_CTX = 0,
	T_DEV,
	T_DEVH,
	T_DEVL,
	T_CFG,
};

struct libusb_destroy_arg {
	bool teardown; // true at instance teardown: guest memory is gone, skip module_free
};

typedef struct devl_payload {
	libusb_device **list;
	uint32_t count;
	uint32_t *devs;
	uint64_t blob;
	wasm_module_inst_t owner;
} devl_payload_t;

typedef struct cfg_payload {
	struct libusb_config_descriptor *cfg;
	uint64_t blob;
	wasm_module_inst_t owner;
} cfg_payload_t;

static void *g_context_key;
static pthread_mutex_t g_table_lock = PTHREAD_MUTEX_INITIALIZER;

static void put_u8le(uint8_t *p, uint8_t v) { p[0] = v; }

static void put_i8le(uint8_t *p, int8_t v) { put_u8le(p, (uint8_t)v); }

static void put_u16le(uint8_t *p, uint16_t v) {
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
}

static void put_u32le(uint8_t *p, uint32_t v) {
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

static void put_i32le(uint8_t *p, int32_t v) { put_u32le(p, (uint32_t)v); }

static void *validate_app_addr_native(wasm_module_inst_t inst, uint32_t app_off, uint32_t size) {
	if (!wasm_runtime_validate_app_addr(inst, app_off, size))
		return NULL;
	return wasm_runtime_addr_app_to_native(inst, app_off);
}

static bool write_u32_to_guest(wasm_module_inst_t inst, uint32_t app_off, uint32_t value) {
	uint8_t *p = validate_app_addr_native(inst, app_off, sizeof(uint32_t));

	if (!p)
		return false;

	put_u32le(p, value);
	return true;
}

static void serialize_device_descriptor(const struct libusb_device_descriptor *desc, uint8_t *p) {
	put_u8le(p + 0, desc->bLength);
	put_u8le(p + 1, desc->bDescriptorType);
	put_u16le(p + 2, desc->bcdUSB);
	put_u8le(p + 4, desc->bDeviceClass);
	put_u8le(p + 5, desc->bDeviceSubClass);
	put_u8le(p + 6, desc->bDeviceProtocol);
	put_u8le(p + 7, desc->bMaxPacketSize0);
	put_u16le(p + 8, desc->idVendor);
	put_u16le(p + 10, desc->idProduct);
	put_u16le(p + 12, desc->bcdDevice);
	put_u8le(p + 14, desc->iManufacturer);
	put_u8le(p + 15, desc->iProduct);
	put_u8le(p + 16, desc->iSerialNumber);
	put_u8le(p + 17, desc->bNumConfigurations);
}

static uint32_t config_blob_size(const struct libusb_config_descriptor *cfg) {
	uint32_t size = W32_SIZEOF_CONFIG_DESCRIPTOR + cfg->bNumInterfaces * W32_SIZEOF_INTERFACE;
	uint32_t i, j;

	for (i = 0; i < cfg->bNumInterfaces; i++) {
		const struct libusb_interface *iface = cfg->interface + i;

		size += (uint32_t)iface->num_altsetting * W32_SIZEOF_INTERFACE_DESCRIPTOR;
		for (j = 0; j < (uint32_t)iface->num_altsetting; j++)
			size += iface->altsetting[j].bNumEndpoints * W32_SIZEOF_ENDPOINT_DESCRIPTOR;
	}

	return size;
}

static void serialize_config_descriptor(const struct libusb_config_descriptor *cfg, uint32_t blob_app, uint8_t *base) {
	uint8_t *p = base + W32_SIZEOF_CONFIG_DESCRIPTOR + cfg->bNumInterfaces * W32_SIZEOF_INTERFACE;
	uint32_t iface_arr_off = W32_SIZEOF_CONFIG_DESCRIPTOR;
	uint32_t i;

	put_u8le(base + 0, cfg->bLength);
	put_u8le(base + 1, cfg->bDescriptorType);
	put_u16le(base + 2, cfg->wTotalLength);
	put_u8le(base + 4, cfg->bNumInterfaces);
	put_u8le(base + 5, cfg->bConfigurationValue);
	put_u8le(base + 6, cfg->iConfiguration);
	put_u8le(base + 7, cfg->bmAttributes);
	put_u8le(base + 8, cfg->MaxPower);
	put_u32le(base + W32_CFG_OFF_interface, blob_app + iface_arr_off);
	put_u32le(base + W32_CFG_OFF_extra, 0);
	put_i32le(base + W32_CFG_OFF_extra_length, 0);

	for (i = 0; i < cfg->bNumInterfaces; i++) {
		const struct libusb_interface *iface = cfg->interface + i;
		uint32_t iface_entry_off = iface_arr_off + i * W32_SIZEOF_INTERFACE;
		uint32_t alt_start = (uint32_t)(p - base);
		uint32_t num_altsetting = (uint32_t)iface->num_altsetting;
		uint8_t *ep_p;
		uint32_t j, k;

		put_u32le(base + iface_entry_off + W32_IFACE_OFF_altsetting, blob_app + alt_start);
		put_i32le(base + iface_entry_off + W32_IFACE_OFF_num_altsetting, iface->num_altsetting);

		for (j = 0; j < num_altsetting; j++) {
			const struct libusb_interface_descriptor *ifd = iface->altsetting + j;

			put_u8le(p + 0, ifd->bLength);
			put_u8le(p + 1, ifd->bDescriptorType);
			put_u8le(p + 2, ifd->bInterfaceNumber);
			put_u8le(p + 3, ifd->bAlternateSetting);
			put_u8le(p + 4, ifd->bNumEndpoints);
			put_u8le(p + 5, ifd->bInterfaceClass);
			put_u8le(p + 6, ifd->bInterfaceSubClass);
			put_u8le(p + 7, ifd->bInterfaceProtocol);
			put_u8le(p + 8, ifd->iInterface);
			put_u32le(p + W32_IFD_OFF_extra, 0);
			put_i32le(p + W32_IFD_OFF_extra_length, 0);
			p += W32_SIZEOF_INTERFACE_DESCRIPTOR;
		}

		ep_p = p;
		for (j = 0; j < num_altsetting; j++) {
			const struct libusb_interface_descriptor *ifd = iface->altsetting + j;
			uint32_t ep_base = (uint32_t)(ep_p - base);

			for (k = 0; k < ifd->bNumEndpoints; k++) {
				const struct libusb_endpoint_descriptor *epd = ifd->endpoint + k;

				put_u8le(ep_p + 0, epd->bLength);
				put_u8le(ep_p + 1, epd->bDescriptorType);
				put_u8le(ep_p + 2, epd->bEndpointAddress);
				put_u8le(ep_p + 3, epd->bmAttributes);
				put_u16le(ep_p + 4, epd->wMaxPacketSize);
				put_u8le(ep_p + 6, epd->bInterval);
				put_u8le(ep_p + 7, epd->bRefresh);
				put_u8le(ep_p + 8, epd->bSynchAddress);
				put_u32le(ep_p + W32_EPD_OFF_extra, 0);
				put_i32le(ep_p + W32_EPD_OFF_extra_length, 0);
				ep_p += W32_SIZEOF_ENDPOINT_DESCRIPTOR;
			}

			put_u32le(base + alt_start + j * W32_SIZEOF_INTERFACE_DESCRIPTOR + W32_IFD_OFF_endpoint, blob_app + ep_base);
		}
		p = ep_p;
	}
}

// Destroy callbacks of the pointer_table entries. user_refs_left > 0 means the entry died with guest references
// still outstanding (e.g. instance teardown); arg->teardown means guest memory is already gone and blobs must not
// be freed.
static void destroy_ctx(void *payload, uint32_t user_refs_left, void *destroy_arg) {
	(void)user_refs_left;
	(void)destroy_arg;

	libusb_exit((libusb_context *)payload);
}

static void destroy_dev(void *payload, uint32_t user_refs_left, void *destroy_arg) {
	(void)destroy_arg;

	if (user_refs_left > 0)
		libusb_unref_device((libusb_device *)payload);
}

static void destroy_devh(void *payload, uint32_t user_refs_left, void *destroy_arg) {
	(void)user_refs_left;
	(void)destroy_arg;

	libusb_close((libusb_device_handle *)payload);
}

static void destroy_devl(void *payload, uint32_t user_refs_left, void *destroy_arg) {
	devl_payload_t *p = (devl_payload_t *)payload;
	const struct libusb_destroy_arg *arg = (const struct libusb_destroy_arg *)destroy_arg;

	(void)user_refs_left;

	if (p->list)
		libusb_free_device_list(p->list, 0);
	if (p->devs)
		wasm_runtime_free(p->devs);
	if (p->blob && p->owner && !arg->teardown)
		wasm_runtime_module_free(p->owner, p->blob);
	wasm_runtime_free(p);
}

static void destroy_cfg(void *payload, uint32_t user_refs_left, void *destroy_arg) {
	cfg_payload_t *p = (cfg_payload_t *)payload;
	const struct libusb_destroy_arg *arg = (const struct libusb_destroy_arg *)destroy_arg;

	(void)user_refs_left;

	if (p->cfg)
		libusb_free_config_descriptor(p->cfg);
	if (p->blob && p->owner && !arg->teardown)
		wasm_runtime_module_free(p->owner, p->blob);
	wasm_runtime_free(p);
}

static void table_context_dtor(wasm_module_inst_t inst, void *ctx) {
	struct libusb_destroy_arg arg = { .teardown = true };

	(void)inst;

	pointer_table_destroy((struct PointerTable *)ctx, &arg);
}

static struct PointerTable *get_table(wasm_module_inst_t inst) {
	struct PointerTable *table = (struct PointerTable *)wasm_runtime_get_context(inst, g_context_key);

	if (table)
		return table;

	if (!(table = pointer_table_create()))
		return NULL;

	// lazily create the per instance table following the documented
	// double-checked pattern of wasm_runtime_create_context_key()
	pthread_mutex_lock(&g_table_lock);
	if (wasm_runtime_get_context(inst, g_context_key)) {
		// another thread won the race
		pthread_mutex_unlock(&g_table_lock);
		{
			struct libusb_destroy_arg arg = { .teardown = false };

			pointer_table_destroy(table, &arg);
		}
		return (struct PointerTable *)wasm_runtime_get_context(inst, g_context_key);
	}
	wasm_runtime_set_context_spread(inst, g_context_key, table);
	pthread_mutex_unlock(&g_table_lock);

	return table;
}

// like get_table(), but never creates a table: used by wrappers that only look
// up or remove entries, where an absent table simply means "invalid handle"
static struct PointerTable *peek_table(wasm_module_inst_t inst) {
	return (struct PointerTable *)wasm_runtime_get_context(inst, g_context_key);
}

static int32_t libusb_init_wrapper(wasm_exec_env_t exec_env, uint32_t slot) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	libusb_context *ctx = NULL;
	uint32_t key;
	int32_t r;

	if (slot == 0)
		return (int32_t)libusb_init(NULL);

	r = (int32_t)libusb_init(&ctx);
	if (r != LIBUSB_SUCCESS)
		return r;

	if (!(table = get_table(inst))) {
		libusb_exit(ctx);
		return (int32_t)LIBUSB_ERROR_NO_MEM;
	}

	if (!(key = pointer_table_add(table, T_CTX, destroy_ctx, ctx))) {
		libusb_exit(ctx);
		return (int32_t)LIBUSB_ERROR_NO_MEM;
	}

	if (!write_u32_to_guest(inst, slot, key)) {
		pointer_table_remove(table, key, &(struct libusb_destroy_arg){ .teardown = false });
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
	}

	return LIBUSB_SUCCESS;
}

static void libusb_exit_wrapper(wasm_exec_env_t exec_env, uint32_t ctx_ptr) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;

	if (ctx_ptr == 0) {
		libusb_exit(NULL);
		return;
	}

	if (!(table = peek_table(inst)))
		return;

	if (!pointer_table_remove(table, ctx_ptr, &(struct libusb_destroy_arg){ .teardown = false }))
		pak_global_log("libusb_exit: invalid context handle %u", ctx_ptr);
}

static void libusb_set_debug_wrapper(wasm_exec_env_t exec_env, uint32_t ctx_ptr, uint32_t level) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *entry;

	if (ctx_ptr == 0) {
		libusb_set_debug(NULL, (int)level);
		return;
	}

	if (!(table = peek_table(inst)))
		return;

	if (!(entry = pointer_table_acquire(table, ctx_ptr, T_CTX))) {
		pak_global_log("libusb_set_debug: invalid context handle %u", ctx_ptr);
		return;
	}

	libusb_set_debug((libusb_context *)pointer_table_payload(entry), (int)level);
	pointer_table_release(entry);
}

static int32_t libusb_get_device_list_wrapper(wasm_exec_env_t exec_env, uint32_t ctx_ptr, uint32_t slot) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *ctx_entry = NULL;
	libusb_context *ctx = NULL;
	libusb_device **list = NULL;
	uint8_t *blob_native = NULL;
	uint64_t blob = 0;
	devl_payload_t *devl = NULL;
	uint32_t key = 0;
	ssize_t count;
	uint32_t i;
	int32_t ret = LIBUSB_SUCCESS;
	struct libusb_destroy_arg arg = { .teardown = false };

	if (!(table = get_table(inst)))
		return (int32_t)LIBUSB_ERROR_NO_MEM;

	if (ctx_ptr != 0) {
		if (!(ctx_entry = pointer_table_acquire(table, ctx_ptr, T_CTX))) {
			pak_global_log("libusb_get_device_list: invalid context handle %u", ctx_ptr);
			return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
		}
		ctx = (libusb_context *)pointer_table_payload(ctx_entry);
	}

	count = libusb_get_device_list(ctx, &list);
	if (count < 0) {
		ret = (int32_t)count;
		goto fail;
	}

	blob = wasm_runtime_module_malloc(inst, (uint64_t)((uint32_t)count + 1) * sizeof(uint32_t), (void **)&blob_native);
	if (!blob) {
		ret = (int32_t)LIBUSB_ERROR_NO_MEM;
		goto fail;
	}

	if (!(devl = (devl_payload_t *)wasm_runtime_malloc(sizeof(devl_payload_t)))) {
		ret = (int32_t)LIBUSB_ERROR_NO_MEM;
		goto fail;
	}
	memset(devl, 0, sizeof(*devl));
	devl->list = list;
	devl->blob = blob;
	devl->owner = inst;

	if (!(key = pointer_table_add_with_key(table, (uint32_t)blob, T_DEVL, destroy_devl, devl))) {
		ret = (int32_t)LIBUSB_ERROR_NO_MEM;
		goto fail;
	}

	if (!(devl->devs = (uint32_t *)wasm_runtime_malloc(sizeof(uint32_t) * ((uint32_t)count + 1)))) {
		ret = (int32_t)LIBUSB_ERROR_NO_MEM;
		goto fail;
	}

	for (i = 0; i < (uint32_t)count; i++) {
		uint32_t dev_key = pointer_table_add(table, T_DEV, destroy_dev, libusb_ref_device(list[i]));

		if (!dev_key) {
			ret = (int32_t)LIBUSB_ERROR_NO_MEM;
			goto fail;
		}
		devl->devs[i] = dev_key;
		put_u32le(blob_native + i * sizeof(uint32_t), dev_key);
		devl->count = i + 1;
	}
	put_u32le(blob_native + (uint32_t)count * sizeof(uint32_t), 0);

	if (!write_u32_to_guest(inst, slot, (uint32_t)blob)) {
		ret = (int32_t)LIBUSB_ERROR_INVALID_PARAM;
		goto fail;
	}

	pointer_table_release(ctx_entry);
	return (int32_t)count;

fail:
	if (key) {
		// release the host list ourselves (unref_devices=1) so that devices that did not make it into the table are
		// unref'd as well, then let the destroy callback free the rest
		if (devl->list) {
			libusb_free_device_list(devl->list, 1);
			devl->list = NULL;
		}
		for (i = 0; i < devl->count; i++) {
			struct PtNode *dev_entry = pointer_table_acquire(table, devl->devs[i], T_DEV);

			if (!dev_entry)
				continue;
			if (pointer_table_user_unref(table, devl->devs[i], &arg) > 0)
				libusb_unref_device((libusb_device *)pointer_table_payload(dev_entry));
			pointer_table_release(dev_entry);
		}
		pointer_table_remove(table, key, &arg);
	}
	if (blob)
		wasm_runtime_module_free(inst, blob);
	pointer_table_release(ctx_entry);
	return ret;
}

static void libusb_free_device_list_wrapper(wasm_exec_env_t exec_env, uint32_t list_ptr, uint32_t unref_devices) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *devl_entry;
	devl_payload_t *devl;
	uint32_t i;

	if (!(table = peek_table(inst)))
		return;

	if (!(devl_entry = pointer_table_acquire(table, list_ptr, T_DEVL))) {
		pak_global_log("libusb_free_device_list: invalid list handle %u", list_ptr);
		return;
	}

	devl = (devl_payload_t *)pointer_table_payload(devl_entry);

	if (unref_devices) {
		for (i = 0; i < devl->count; i++) {
			struct PtNode *dev_entry = pointer_table_acquire(table, devl->devs[i], T_DEV);
			libusb_device *dev;
			uint32_t prev;

			if (!dev_entry)
				continue;

			dev = (libusb_device *)pointer_table_payload(dev_entry);
			prev = pointer_table_user_unref(table, devl->devs[i], &(struct libusb_destroy_arg){ .teardown = false });
			if (prev > 0)
				libusb_unref_device(dev);
			pointer_table_release(dev_entry);
		}
		devl->count = 0;
	}

	pointer_table_release(devl_entry);

	if (!pointer_table_remove(table, list_ptr, &(struct libusb_destroy_arg){ .teardown = false }))
		pak_global_log("libusb_free_device_list: list handle %u vanished", list_ptr);
}

static int32_t libusb_ref_device_wrapper(wasm_exec_env_t exec_env, uint32_t dev_ptr) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *entry;
	libusb_device *dev;
	uint32_t n;

	if (!(table = peek_table(inst)))
		return 0;

	if (!(entry = pointer_table_acquire(table, dev_ptr, T_DEV))) {
		pak_global_log("libusb_ref_device: invalid device handle %u", dev_ptr);
		return 0;
	}

	n = pointer_table_user_ref(table, dev_ptr);
	if (n > 0)
		libusb_ref_device((libusb_device *)pointer_table_payload(entry));

	pointer_table_release(entry);
	return n > 0 ? (int32_t)dev_ptr : 0;
}

static void libusb_unref_device_wrapper(wasm_exec_env_t exec_env, uint32_t dev_ptr) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *entry;
	libusb_device *dev;
	uint32_t prev;

	if (!(table = peek_table(inst)))
		return;

	if (!(entry = pointer_table_acquire(table, dev_ptr, T_DEV))) {
		pak_global_log("libusb_unref_device: invalid device handle %u", dev_ptr);
		return;
	}

	dev = (libusb_device *)pointer_table_payload(entry);
	prev = pointer_table_user_unref(table, dev_ptr, &(struct libusb_destroy_arg){ .teardown = false });
	if (prev > 0)
		libusb_unref_device(dev);

	pointer_table_release(entry);
}

static int32_t libusb_open_wrapper(wasm_exec_env_t exec_env, uint32_t dev_ptr, uint32_t slot) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *dev_entry;
	libusb_device_handle *dev_handle = NULL;
	uint32_t key;
	int32_t r;

	if (!(table = get_table(inst)))
		return (int32_t)LIBUSB_ERROR_NO_MEM;

	if (!(dev_entry = pointer_table_acquire(table, dev_ptr, T_DEV))) {
		pak_global_log("libusb_open: invalid device handle %u", dev_ptr);
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
	}

	r = (int32_t)libusb_open((libusb_device *)pointer_table_payload(dev_entry), &dev_handle);
	if (r != LIBUSB_SUCCESS) {
		pointer_table_release(dev_entry);
		return r;
	}

	if (!(key = pointer_table_add(table, T_DEVH, destroy_devh, dev_handle))) {
		libusb_close(dev_handle);
		pointer_table_release(dev_entry);
		return (int32_t)LIBUSB_ERROR_NO_MEM;
	}

	if (!write_u32_to_guest(inst, slot, key)) {
		pointer_table_remove(table, key, &(struct libusb_destroy_arg){ .teardown = false });
		pointer_table_release(dev_entry);
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
	}

	pointer_table_release(dev_entry);
	return LIBUSB_SUCCESS;
}

static void libusb_close_wrapper(wasm_exec_env_t exec_env, uint32_t devh_ptr) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;

	if (!(table = peek_table(inst)))
		return;

	if (!pointer_table_remove(table, devh_ptr, &(struct libusb_destroy_arg){ .teardown = false }))
		pak_global_log("libusb_close: invalid device handle %u", devh_ptr);
}

static int32_t libusb_claim_interface_wrapper(wasm_exec_env_t exec_env, uint32_t devh_ptr, uint32_t interface_number) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *entry;
	int32_t r;

	if (!(table = peek_table(inst)))
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;

	if (!(entry = pointer_table_acquire(table, devh_ptr, T_DEVH))) {
		pak_global_log("libusb_claim_interface: invalid device handle %u", devh_ptr);
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
	}

	r = (int32_t)libusb_claim_interface((libusb_device_handle *)pointer_table_payload(entry), (int)interface_number);
	pointer_table_release(entry);
	return r;
}

static int32_t libusb_release_interface_wrapper(wasm_exec_env_t exec_env, uint32_t devh_ptr, uint32_t interface_number) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *entry;
	int32_t r;

	if (!(table = peek_table(inst)))
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;

	if (!(entry = pointer_table_acquire(table, devh_ptr, T_DEVH))) {
		pak_global_log("libusb_release_interface: invalid device handle %u", devh_ptr);
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
	}

	r = (int32_t)libusb_release_interface((libusb_device_handle *)pointer_table_payload(entry), (int)interface_number);
	pointer_table_release(entry);
	return r;
}

static int32_t libusb_set_auto_detach_kernel_driver_wrapper(wasm_exec_env_t exec_env, uint32_t devh_ptr, uint32_t enable) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *entry;
	int32_t r;

	if (!(table = peek_table(inst)))
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;

	if (!(entry = pointer_table_acquire(table, devh_ptr, T_DEVH))) {
		pak_global_log("libusb_set_auto_detach_kernel_driver: invalid device handle %u", devh_ptr);
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
	}

	r = (int32_t)libusb_set_auto_detach_kernel_driver((libusb_device_handle *)pointer_table_payload(entry), (int)enable);
	pointer_table_release(entry);
	return r;
}

static int32_t libusb_set_interface_alt_setting_wrapper(wasm_exec_env_t exec_env, uint32_t devh_ptr,
                                                        uint32_t interface_number, uint32_t alternate_setting) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *entry;
	int32_t r;

	if (!(table = peek_table(inst)))
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;

	if (!(entry = pointer_table_acquire(table, devh_ptr, T_DEVH))) {
		pak_global_log("libusb_set_interface_alt_setting: invalid device handle %u", devh_ptr);
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
	}

	r = (int32_t)libusb_set_interface_alt_setting((libusb_device_handle *)pointer_table_payload(entry),
	                                              (int)interface_number, (int)alternate_setting);
	pointer_table_release(entry);
	return r;
}

static int32_t libusb_clear_halt_wrapper(wasm_exec_env_t exec_env, uint32_t devh_ptr, uint32_t endpoint) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *entry;
	int32_t r;

	if (!(table = peek_table(inst)))
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;

	if (!(entry = pointer_table_acquire(table, devh_ptr, T_DEVH))) {
		pak_global_log("libusb_clear_halt: invalid device handle %u", devh_ptr);
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
	}

	r = (int32_t)libusb_clear_halt((libusb_device_handle *)pointer_table_payload(entry), (uint8_t)endpoint);
	pointer_table_release(entry);
	return r;
}

static int32_t libusb_get_device_descriptor_wrapper(wasm_exec_env_t exec_env, uint32_t dev_ptr, uint32_t desc) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *entry;
	struct libusb_device_descriptor host_desc;
	uint8_t *p;
	int32_t r;

	if (!(table = peek_table(inst)))
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;

	if (!(entry = pointer_table_acquire(table, dev_ptr, T_DEV))) {
		pak_global_log("libusb_get_device_descriptor: invalid device handle %u", dev_ptr);
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
	}

	r = LIBUSB_ERROR_INVALID_PARAM;
	if (!(p = validate_app_addr_native(inst, desc, W32_SIZEOF_DEVICE_DESCRIPTOR)))
		goto out;

	if ((r = (int32_t)libusb_get_device_descriptor((libusb_device *)pointer_table_payload(entry), &host_desc))
	    == LIBUSB_SUCCESS)
		serialize_device_descriptor(&host_desc, p);

out:
	pointer_table_release(entry);
	return r;
}

static int32_t libusb_get_config_descriptor_wrapper(wasm_exec_env_t exec_env, uint32_t dev_ptr, uint32_t config_index,
                                                    uint32_t slot) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *dev_entry;
	cfg_payload_t *cfg_payload;
	struct libusb_config_descriptor *cfg = NULL;
	uint8_t *blob_native = NULL;
	uint64_t blob = 0;
	uint32_t key;
	int32_t ret;

	if (!(table = get_table(inst)))
		return (int32_t)LIBUSB_ERROR_NO_MEM;

	if (!(dev_entry = pointer_table_acquire(table, dev_ptr, T_DEV))) {
		pak_global_log("libusb_get_config_descriptor: invalid device handle %u", dev_ptr);
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
	}

	ret = (int32_t)libusb_get_config_descriptor((libusb_device *)pointer_table_payload(dev_entry),
	                                            (uint8_t)config_index, &cfg);
	if (ret != LIBUSB_SUCCESS) {
		pointer_table_release(dev_entry);
		return ret;
	}

	if (!(blob = wasm_runtime_module_malloc(inst, config_blob_size(cfg), (void **)&blob_native))) {
		libusb_free_config_descriptor(cfg);
		pointer_table_release(dev_entry);
		return (int32_t)LIBUSB_ERROR_NO_MEM;
	}

	serialize_config_descriptor(cfg, (uint32_t)blob, blob_native);

	if (!(cfg_payload = (cfg_payload_t *)wasm_runtime_malloc(sizeof(cfg_payload_t)))) {
		wasm_runtime_module_free(inst, blob);
		libusb_free_config_descriptor(cfg);
		pointer_table_release(dev_entry);
		return (int32_t)LIBUSB_ERROR_NO_MEM;
	}
	cfg_payload->cfg = cfg;
	cfg_payload->blob = blob;
	cfg_payload->owner = inst;

	if (!(key = pointer_table_add_with_key(table, (uint32_t)blob, T_CFG, destroy_cfg, cfg_payload))) {
		wasm_runtime_free(cfg_payload);
		wasm_runtime_module_free(inst, blob);
		libusb_free_config_descriptor(cfg);
		pointer_table_release(dev_entry);
		return (int32_t)LIBUSB_ERROR_NO_MEM;
	}

	if (!write_u32_to_guest(inst, slot, key)) {
		pointer_table_remove(table, key, &(struct libusb_destroy_arg){ .teardown = false });
		pointer_table_release(dev_entry);
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
	}

	pointer_table_release(dev_entry);
	return LIBUSB_SUCCESS;
}

static void libusb_free_config_descriptor_wrapper(wasm_exec_env_t exec_env, uint32_t cfg_ptr) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;

	if (!(table = peek_table(inst)))
		return;

	if (!pointer_table_remove(table, cfg_ptr, &(struct libusb_destroy_arg){ .teardown = false }))
		pak_global_log("libusb_free_config_descriptor: invalid descriptor handle %u", cfg_ptr);
}

static int32_t libusb_get_max_packet_size_wrapper(wasm_exec_env_t exec_env, uint32_t dev_ptr, uint32_t endpoint) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *entry;
	int32_t r;

	if (!(table = peek_table(inst)))
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;

	if (!(entry = pointer_table_acquire(table, dev_ptr, T_DEV))) {
		pak_global_log("libusb_get_max_packet_size: invalid device handle %u", dev_ptr);
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
	}

	r = (int32_t)libusb_get_max_packet_size((libusb_device *)pointer_table_payload(entry), (uint8_t)endpoint);
	pointer_table_release(entry);
	return r;
}

static int32_t transfer_common(wasm_exec_env_t exec_env, uint32_t devh_ptr, uint32_t endpoint, uint32_t data, uint32_t length,
                               uint32_t transferred_slot, uint32_t timeout, bool interrupt) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *entry;
	// libusb_device_handle *dev_handle;
	uint8_t *staging = NULL;
	uint8_t *guest_buf;
	int transferred = 0;
	int r;

	if (length > INT_MAX)
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;

	if (!(table = peek_table(inst)))
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;

	if (!(entry = pointer_table_acquire(table, devh_ptr, T_DEVH))) {
		pak_global_log("transfer: invalid device handle %u", devh_ptr);
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
	}

	r = LIBUSB_ERROR_INVALID_PARAM;
	if (length > 0) {
		if (!(guest_buf = validate_app_addr_native(inst, data, length)))
			goto out;
		if (!(staging = (uint8_t *)wasm_runtime_malloc(length)))
			goto out;
		memcpy(staging, guest_buf, length);
	}

	if (!wasm_runtime_validate_app_addr(inst, transferred_slot, sizeof(uint32_t)))
		goto out;

	if (interrupt)
		r = libusb_interrupt_transfer((libusb_device_handle *)pointer_table_payload(entry), (uint8_t)endpoint, staging,
		                              (int)length, &transferred, (unsigned)timeout);
	else
		r = libusb_bulk_transfer((libusb_device_handle *)pointer_table_payload(entry), (uint8_t)endpoint, staging,
		                         (int)length, &transferred, (unsigned)timeout);

	if (transferred > 0 && (endpoint & LIBUSB_ENDPOINT_IN) && length > 0) {
		guest_buf = (uint8_t *)wasm_runtime_addr_app_to_native(inst, data);
		if (guest_buf)
			memcpy(guest_buf, staging, (uint32_t)transferred);
	}

	{
		uint8_t *tp = (uint8_t *)wasm_runtime_addr_app_to_native(inst, transferred_slot);

		if (tp)
			put_u32le(tp, (uint32_t)transferred);
	}

out:
	if (staging)
		wasm_runtime_free(staging);
	pointer_table_release(entry);
	return (int32_t)r;
}

static int32_t libusb_bulk_transfer_wrapper(wasm_exec_env_t exec_env, uint32_t devh_ptr, uint32_t endpoint, uint32_t data,
                                            uint32_t length, uint32_t transferred_slot, uint32_t timeout) {
	return transfer_common(exec_env, devh_ptr, endpoint, data, length, transferred_slot, timeout, false);
}

static int32_t libusb_interrupt_transfer_wrapper(wasm_exec_env_t exec_env, uint32_t devh_ptr, uint32_t endpoint,
                                                 uint32_t data, uint32_t length, uint32_t transferred_slot, uint32_t timeout) {
	return transfer_common(exec_env, devh_ptr, endpoint, data, length, transferred_slot, timeout, true);
}

static int32_t libusb_control_transfer_wrapper(wasm_exec_env_t exec_env, uint32_t devh_ptr, uint32_t bm_request_type,
                                               uint32_t b_request, uint32_t w_value, uint32_t w_index, uint32_t data,
                                               uint32_t w_length, uint32_t timeout) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct PointerTable *table;
	struct PtNode *entry;
	uint8_t *staging = NULL;
	uint8_t *guest_buf;
	int32_t r;

	if (w_length > INT_MAX || w_length > UINT16_MAX)
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;

	if (!(table = peek_table(inst)))
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;

	if (!(entry = pointer_table_acquire(table, devh_ptr, T_DEVH))) {
		pak_global_log("libusb_control_transfer: invalid device handle %u", devh_ptr);
		return (int32_t)LIBUSB_ERROR_INVALID_PARAM;
	}

	r = LIBUSB_ERROR_INVALID_PARAM;
	if (w_length > 0) {
		if (!(guest_buf = validate_app_addr_native(inst, data, w_length)))
			goto out;
		if (!(staging = (uint8_t *)wasm_runtime_malloc(w_length)))
			goto out;
		memcpy(staging, guest_buf, w_length);
	}

	r = (int32_t)libusb_control_transfer((libusb_device_handle *)pointer_table_payload(entry), (uint8_t)bm_request_type,
	                                     (uint8_t)b_request, (uint16_t)w_value, (uint16_t)w_index, staging,
	                                     (uint16_t)w_length, (unsigned)timeout);

	if (r > 0 && w_length > 0) {
		guest_buf = (uint8_t *)wasm_runtime_addr_app_to_native(inst, data);
		if (guest_buf)
			memcpy(guest_buf, staging, (uint32_t)r);
	}

out:
	if (staging)
		wasm_runtime_free(staging);
	pointer_table_release(entry);
	return r;
}

#define REG_NATIVE_FUNC(func_name, signature) { #func_name, (void *)(uintptr_t)func_name##_wrapper, signature, NULL }

static NativeSymbol native_symbols_libusb[] = {
	REG_NATIVE_FUNC(libusb_init, "(i)i"),
	REG_NATIVE_FUNC(libusb_exit, "(i)"),
	REG_NATIVE_FUNC(libusb_set_debug, "(ii)"),
	REG_NATIVE_FUNC(libusb_get_device_list, "(ii)i"),
	REG_NATIVE_FUNC(libusb_free_device_list, "(ii)"),
	REG_NATIVE_FUNC(libusb_ref_device, "(i)i"),
	REG_NATIVE_FUNC(libusb_unref_device, "(i)"),
	REG_NATIVE_FUNC(libusb_open, "(ii)i"),
	REG_NATIVE_FUNC(libusb_close, "(i)"),
	REG_NATIVE_FUNC(libusb_claim_interface, "(ii)i"),
	REG_NATIVE_FUNC(libusb_release_interface, "(ii)i"),
	REG_NATIVE_FUNC(libusb_set_auto_detach_kernel_driver, "(ii)i"),
	REG_NATIVE_FUNC(libusb_set_interface_alt_setting, "(iii)i"),
	REG_NATIVE_FUNC(libusb_clear_halt, "(ii)i"),
	REG_NATIVE_FUNC(libusb_get_device_descriptor, "(ii)i"),
	REG_NATIVE_FUNC(libusb_get_config_descriptor, "(iii)i"),
	REG_NATIVE_FUNC(libusb_free_config_descriptor, "(i)"),
	REG_NATIVE_FUNC(libusb_get_max_packet_size, "(ii)i"),
	REG_NATIVE_FUNC(libusb_bulk_transfer, "(iiiiii)i"),
	REG_NATIVE_FUNC(libusb_interrupt_transfer, "(iiiiii)i"),
	REG_NATIVE_FUNC(libusb_control_transfer, "(iiiiiiii)i"),
};

bool libusb_native_register(void) {
	if (g_context_key)
		return true;

	if (!(g_context_key = wasm_runtime_create_context_key(table_context_dtor)))
		return false;

	if (!wasm_runtime_register_natives("env", native_symbols_libusb,
	                                   sizeof(native_symbols_libusb) / sizeof(NativeSymbol))) {
		wasm_runtime_destroy_context_key(g_context_key);
		g_context_key = NULL;
		return false;
	}

	return true;
}

void libusb_native_unregister(void) {
	if (!g_context_key)
		return;

	// the tables themselves are destroyed by the instance context destructor,
	// the host must deinstantiate all module instances before this call
	wasm_runtime_destroy_context_key(g_context_key);
	g_context_key = NULL;
}

void libusb_native_release_instance(wasm_module_inst_t module_inst) {
	struct PointerTable *table = (struct PointerTable *)wasm_runtime_get_context(module_inst, g_context_key);
	struct libusb_destroy_arg arg = { .teardown = false };

	if (!table)
		return;

	wasm_runtime_set_context_spread(module_inst, g_context_key, NULL);
	pointer_table_destroy(table, &arg);
}

int32_t get_libusb_export_apis(NativeSymbol **funcs) {
	*funcs = native_symbols_libusb;
	return sizeof(native_symbols_libusb) / sizeof(NativeSymbol);
}
