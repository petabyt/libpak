/* wasm32 Bluetooth bridge. Guest-visible structures live in linear memory;
 * native objects are kept in a per-instance typed pointer table. */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <bluetooth.h>
#include "main.h"
#include "layout.h"
#include "memory.h"
#include "pointer_table.h"

enum { BT_ADAPTER = 1, BT_DEVICE, BT_SERVICE, BT_CHARACTERISTIC };
struct bt_state;

struct bt_object {
	struct bt_state *state;
	struct bt_object *next;
	void *native;
	uint32_t guest, type, callback, callback_arg, generation;
};

struct bt_event {
	uint32_t device, chr, callback, arg, type, generation;
};

struct bt_state {
	struct bt_state *next;
	wasm_module_inst_t inst;
	struct PakBt *bt;
	int owned;
	struct PointerTable *table;
	struct bt_object *objects;
	struct bt_event events[128];
	unsigned head, count;
	uint32_t generation;
	pthread_mutex_t lock;
	pthread_cond_t wake;
	wasm_thread_t worker;
	wasm_module_inst_t worker_inst;
	int worker_started, stopping;
};
static struct bt_state *states;
static pthread_mutex_t states_lock = PTHREAD_MUTEX_INITIALIZER;

static struct bt_state *get_state(wasm_exec_env_t env) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
	pthread_mutex_lock(&states_lock);
	for (struct bt_state *s = states; s; s = s->next)
		if (s->inst == inst || s->worker_inst == inst) {
			pthread_mutex_unlock(&states_lock);
			return s;
		}
	pthread_mutex_unlock(&states_lock);
	return NULL;
}

bool wasm_bt_attach(wasm_module_inst_t inst, struct PakBt *bt) {
	struct bt_state *s = calloc(1, sizeof(*s));
	if (!s)
		return false;
	s->table = pointer_table_create();
	if (!s->table) {
		free(s);
		return false;
	}
	s->inst = inst;
	s->bt = bt;
	if (pthread_mutex_init(&s->lock, NULL)) {
		pointer_table_destroy(s->table, NULL);
		free(s);
		return false;
	}
	if (pthread_cond_init(&s->wake, NULL)) {
		pthread_mutex_destroy(&s->lock);
		pointer_table_destroy(s->table, NULL);
		free(s);
		return false;
	}
	pthread_mutex_lock(&states_lock);
	s->next = states;
	states = s;
	pthread_mutex_unlock(&states_lock);
	return true;
}

static void destroy_object(void *payload, uint32_t refs, void *arg) {
	(void)refs;
	(void)arg;
	struct bt_object *o = payload;
	struct bt_state *s = o->state;
	switch (o->type) {
	case BT_ADAPTER:
		pak_bt_unref_adapter(s->bt, o->native);
		break;
	case BT_DEVICE:
		pak_bt_unref_device(s->bt, o->native);
		break;
	case BT_SERVICE:
		pak_bt_unref_gatt_service(s->bt, o->native);
		break;
	case BT_CHARACTERISTIC:
		pak_bt_unref_gatt_characteristic(s->bt, o->native);
		break;
	}
	wasm_runtime_module_free(s->inst, o->guest);
	free(o);
}

void wasm_bt_detach(wasm_module_inst_t inst) {
	pthread_mutex_lock(&states_lock);
	struct bt_state **p = &states;
	while (*p && (*p)->inst != inst)
		p = &(*p)->next;
	if (!*p) {
		pthread_mutex_unlock(&states_lock);
		return;
	}
	struct bt_state *s = *p;
	pthread_mutex_unlock(&states_lock);
	if (s->worker_started) {
		pthread_mutex_lock(&s->lock);
		s->stopping = 1;
		pthread_cond_signal(&s->wake);
		pthread_mutex_unlock(&s->lock);
		wasm_runtime_join_thread(s->worker, NULL);
	}
	pthread_mutex_lock(&states_lock);
	p = &states;
	while (*p && *p != s) p = &(*p)->next;
	if (*p) *p = s->next;
	pthread_mutex_unlock(&states_lock);
	pointer_table_destroy(s->table, NULL);
	if (s->owned)
		pak_bt_unref_context(s->bt);
	pthread_cond_destroy(&s->wake);
	pthread_mutex_destroy(&s->lock);
	free(s);
}

static struct bt_state *check(wasm_exec_env_t e, uint32_t token) {
	struct bt_state *s = get_state(e);
	return s && token == 1 && s->bt ? s : NULL;
}

static struct PtNode *acquire(struct bt_state *s, uint32_t guest, uint32_t type) {
	return s ? pointer_table_acquire(s->table, guest, type) : NULL;
}

static uint32_t object_size(uint32_t type) {
	return type == BT_ADAPTER ? WASM_SIZEOF_PAK_BT_ADAPTER
	     : type == BT_DEVICE ? WASM_SIZEOF_PAK_BT_DEVICE
	     : type == BT_SERVICE ? WASM_SIZEOF_PAK_GATT_SERVICE
	     : WASM_SIZEOF_PAK_GATT_CHARACTERISTIC;
}

static void serialize(struct bt_object *o) {
	uint32_t sz = object_size(o->type);
	uint8_t *p = wasm_guest_addr(o->state->inst, o->guest, sz);
	if (!p)
		return;
	memset(p, 0, sz);
	if (o->type == BT_ADAPTER) {
		struct PakBtAdapter *a = o->native;
		memcpy(p + WASM_OFFSETOF_PAK_BT_ADAPTER_ADDRESS, a->address, sizeof(a->address));
		memcpy(p + WASM_OFFSETOF_PAK_BT_ADAPTER_NAME, a->name, sizeof(a->name));
		wasm_put_u32le(p + WASM_OFFSETOF_PAK_BT_ADAPTER_POWERED, a->powered);
	} else if (o->type == BT_DEVICE) {
		struct PakBtDevice *d = o->native;
		p[WASM_OFFSETOF_PAK_BT_DEVICE_IS_CLASSIC] = d->is_classic;
		p[WASM_OFFSETOF_PAK_BT_DEVICE_IS_CONNECTED] = d->is_connected;
		p[WASM_OFFSETOF_PAK_BT_DEVICE_IS_BONDED] = d->is_bonded;
		memcpy(p + WASM_OFFSETOF_PAK_BT_DEVICE_NAME, d->name, sizeof(d->name));
		memcpy(p + WASM_OFFSETOF_PAK_BT_DEVICE_MAC_ADDRESS, d->mac_address, sizeof(d->mac_address));
		wasm_put_u32le(p + WASM_OFFSETOF_PAK_BT_DEVICE_BTCLASS, d->btclass);
	} else if (o->type == BT_SERVICE) {
		struct PakGattService *v = o->native;
		memcpy(p + WASM_OFFSETOF_PAK_GATT_SERVICE_UUID, v->uuid, sizeof(v->uuid));
		wasm_put_u16le(p + WASM_OFFSETOF_PAK_GATT_SERVICE_HANDLE, v->handle);
	} else {
		struct PakGattCharacteristic *v = o->native;
		wasm_put_u32le(p + WASM_OFFSETOF_PAK_GATT_CHARACTERISTIC_FLAGS, v->flags);
		memcpy(p + WASM_OFFSETOF_PAK_GATT_CHARACTERISTIC_UUID, v->uuid, sizeof(v->uuid));
		wasm_put_u16le(p + WASM_OFFSETOF_PAK_GATT_CHARACTERISTIC_HANDLE, v->handle);
		wasm_put_u16le(p + WASM_OFFSETOF_PAK_GATT_CHARACTERISTIC_MTU, v->mtu);
	}
}

static uint32_t add(struct bt_state *s, uint32_t type, void *native) {
	if (!native)
		return 0;
	struct bt_object *o = calloc(1, sizeof(*o));
	if (!o)
		goto fail_native;
	o->state = s;
	o->type = type;
	o->native = native;
	void *p = NULL;
	uint32_t size = object_size(type);
	o->guest = wasm_runtime_module_malloc(s->inst, size, &p);
	if (!o->guest) {
		free(o);
		goto fail_native;
	}
	if (!pointer_table_add_with_key(s->table, o->guest, type, destroy_object, o)) {
		wasm_runtime_module_free(s->inst, o->guest);
		free(o);
		goto fail_native;
	}
	pthread_mutex_lock(&s->lock);
	o->next = s->objects;
	s->objects = o;
	pthread_mutex_unlock(&s->lock);
	serialize(o);
	return o->guest;
fail_native:
	if (type == BT_ADAPTER)
		pak_bt_unref_adapter(s->bt, native);
	else if (type == BT_DEVICE)
		pak_bt_unref_device(s->bt, native);
	else if (type == BT_SERVICE)
		pak_bt_unref_gatt_service(s->bt, native);
	else
		pak_bt_unref_gatt_characteristic(s->bt, native);
	return 0;
}

static int remove_obj(struct bt_state *s, uint32_t guest, uint32_t type) {
	struct PtNode *n = acquire(s, guest, type);
	if (!n)
		return -1;
	struct bt_object *o = pointer_table_payload(n);
	pthread_mutex_lock(&s->lock);
	if (o->type == BT_DEVICE && o->callback) {
		pthread_mutex_unlock(&s->lock);
		pointer_table_release(n);
		return -1;
	}
	struct bt_object **p = &s->objects;
	while (*p && *p != o)
		p = &(*p)->next;
	if (*p)
		*p = o->next;
	pthread_mutex_unlock(&s->lock);
	pointer_table_user_unref(s->table, guest, NULL);
	pointer_table_release(n);
	return 0;
}

/* Backend callbacks only enqueue; the worker owns its WAMR execution environment. */
static int on_event(struct PakBt *ctx, enum PakBtEvent type, struct PakBtDevice *dev, struct PakGattCharacteristic *chr,
					void *arg) {
	(void)ctx;
	(void)dev;
	struct bt_object *o = arg;
	struct bt_state *s = o->state;
	uint32_t guest_chr = 0;
	pthread_mutex_lock(&s->lock);
	if (s->stopping) {
		pthread_mutex_unlock(&s->lock);
		return -1;
	}
	for (struct bt_object *c = s->objects; c; c = c->next)
		if (c->type == BT_CHARACTERISTIC && c->native == chr) {
			guest_chr = c->guest;
			break;
		}
	if (s->count == 128) {
		pthread_mutex_unlock(&s->lock);
		return -1;
	}
	unsigned i = (s->head + s->count++) % 128;
	s->events[i] = (struct bt_event){o->guest, guest_chr, o->callback, o->callback_arg, type, o->generation};
	pthread_cond_signal(&s->wake);
	pthread_mutex_unlock(&s->lock);
	return 0;
}

static void *bt_worker(wasm_exec_env_t env, void *arg) {
	struct bt_state *s = arg;
	pthread_mutex_lock(&states_lock);
	s->worker_inst = wasm_runtime_get_module_inst(env);
	pthread_mutex_unlock(&states_lock);
	for (;;) {
		pthread_mutex_lock(&s->lock);
		while (!s->count && !s->stopping)
			pthread_cond_wait(&s->wake, &s->lock);
		if (s->stopping) {
			pthread_mutex_unlock(&s->lock);
			break;
		}
		struct bt_event ev = s->events[s->head];
		s->head = (s->head + 1) % 128;
		s->count--;
		pthread_mutex_unlock(&s->lock);
		struct PtNode *d = acquire(s, ev.device, BT_DEVICE);
		struct PtNode *c = ev.chr ? acquire(s, ev.chr, BT_CHARACTERISTIC) : NULL;
		if (d) {
			struct bt_object *obj = pointer_table_payload(d);
			pthread_mutex_lock(&s->lock);
			int valid = obj->generation == ev.generation && obj->callback == ev.callback &&
				obj->callback_arg == ev.arg && (!ev.chr || c);
			pthread_mutex_unlock(&s->lock);
			if (valid) {
				uint32_t a[] = {1, ev.type, ev.device, ev.chr, ev.arg};
				if (!wasm_runtime_call_indirect(env, ev.callback, 5, a)) {
					pointer_table_release(c);
					pointer_table_release(d);
					break;
				}
			}
		}
		if (c)
			pointer_table_release(c);
		if (d)
			pointer_table_release(d);
	}
	return NULL;
}

static uint32_t bt_get_context(wasm_exec_env_t e) {
	struct bt_state *s = get_state(e);
	if (!s)
		return 0;
	if (!s->bt) {
		s->bt = pak_bt_get_context();
		s->owned = s->bt != NULL;
	}
	return s->bt ? 1 : 0;
}

static void bt_unref_context(wasm_exec_env_t e, uint32_t token) {
	(void)e;
	(void)token; /* owned until module teardown */
}

static int bt_enabled(wasm_exec_env_t e, uint32_t t) {
	struct bt_state *s = check(e, t);
	return s ? pak_bt_is_enabled(s->bt) : 0;
}

static int bt_supported(wasm_exec_env_t e, uint32_t t, int feat) {
	struct bt_state *s = check(e, t);
	return s ? pak_bt_is_supported(s->bt, feat) : 0;
}

static int bt_n_adapters(wasm_exec_env_t e, uint32_t t) {
	struct bt_state *s = check(e, t);
	return s ? pak_bt_get_n_adapters(s->bt) : -1;
}

static uint32_t bt_adapter(wasm_exec_env_t e, uint32_t t, int i) {
	struct bt_state *s = check(e, t);
	return s ? add(s, BT_ADAPTER, pak_bt_get_adapter(s->bt, i)) : 0;
}

#define UNREF(name, type)                                                                                              \
	static int name(wasm_exec_env_t e, uint32_t t, uint32_t h) {                                                       \
		return remove_obj(check(e, t), h, type);                                                                       \
	}
UNREF(bt_unref_adapter, BT_ADAPTER)
UNREF(bt_unref_device, BT_DEVICE)
UNREF(bt_unref_service, BT_SERVICE)
UNREF(bt_unref_characteristic, BT_CHARACTERISTIC)

static int bt_n_devices(wasm_exec_env_t e, uint32_t t, uint32_t h, int filter) {
	struct bt_state *s = check(e, t);
	struct PtNode *n = acquire(s, h, BT_ADAPTER);
	if (!n)
		return -1;
	struct bt_object *o = pointer_table_payload(n);
	int r = pak_bt_get_n_devices(s->bt, o->native, filter);
	pointer_table_release(n);
	return r;
}

static uint32_t bt_device(wasm_exec_env_t e, uint32_t t, uint32_t h, int index, int filter) {
	struct bt_state *s = check(e, t);
	struct PtNode *n = acquire(s, h, BT_ADAPTER);
	if (!n)
		return 0;
	struct bt_object *o = pointer_table_payload(n);
	void *v = pak_bt_get_device(s->bt, o->native, index, filter);
	pointer_table_release(n);
	return add(s, BT_DEVICE, v);
}

static int bt_device_op(wasm_exec_env_t e, uint32_t t, uint32_t h, int arg, int op) {
	struct bt_state *s = check(e, t);
	struct PtNode *n = acquire(s, h, BT_DEVICE);
	if (!n)
		return -1;
	struct bt_object *o = pointer_table_payload(n);
	int r;
	if (op == 0)
		r = pak_bt_device_update(s->bt, o->native);
	else if (op == 1)
		r = pak_bt_device_connect(s->bt, o->native, arg);
	else if (op == 2)
		r = pak_bt_device_disconnect(s->bt, o->native);
	else
		r = pak_bt_device_create_bond(s->bt, o->native, arg);
	serialize(o);
	pointer_table_release(n);
	return r;
}

static int bt_update(wasm_exec_env_t e, uint32_t t, uint32_t h) {
	return bt_device_op(e, t, h, 0, 0);
}

static int bt_connect(wasm_exec_env_t e, uint32_t t, uint32_t h, int b) {
	return bt_device_op(e, t, h, b, 1);
}

static int bt_disconnect(wasm_exec_env_t e, uint32_t t, uint32_t h) {
	return bt_device_op(e, t, h, 0, 2);
}

static int bt_bond(wasm_exec_env_t e, uint32_t t, uint32_t h, int b) {
	return bt_device_op(e, t, h, b, 3);
}

static int bt_callback(wasm_exec_env_t e, uint32_t t, uint32_t h, uint32_t cb, uint32_t arg) {
	struct bt_state *s = check(e, t);
	struct PtNode *n = acquire(s, h, BT_DEVICE);
	if (!n)
		return -1;
	struct bt_object *o = pointer_table_payload(n);
	wasm_memory_inst_t memory = wasm_runtime_get_default_memory(s->inst);
	int r = -1;
	if (cb && memory && wasm_memory_get_shared(memory) && !o->callback) {
		if (!s->worker_started) {
			if (wasm_runtime_spawn_thread(e, &s->worker, bt_worker, s))
				goto done_callback;
			s->worker_started = 1;
		}
		pthread_mutex_lock(&s->lock);
		o->callback = cb;
		o->callback_arg = arg;
		o->generation = ++s->generation;
		pthread_mutex_unlock(&s->lock);
		r = pak_bt_set_device_callback(s->bt, o->native, on_event, o);
		if (r) {
			pthread_mutex_lock(&s->lock);
			o->callback = 0;
			pthread_mutex_unlock(&s->lock);
		}
	}
done_callback:
	pointer_table_release(n);
	return r;
}

static uint32_t bt_service(wasm_exec_env_t e, uint32_t t, uint32_t h, int index) {
	struct bt_state *s = check(e, t);
	struct PtNode *n = acquire(s, h, BT_DEVICE);
	if (!n)
		return 0;
	struct bt_object *o = pointer_table_payload(n);
	void *v = pak_bt_get_gatt_service(s->bt, o->native, index);
	pointer_table_release(n);
	return add(s, BT_SERVICE, v);
}

static uint32_t bt_characteristic(wasm_exec_env_t e, uint32_t t, uint32_t h, int index) {
	struct bt_state *s = check(e, t);
	struct PtNode *n = acquire(s, h, BT_SERVICE);
	if (!n)
		return 0;
	struct bt_object *o = pointer_table_payload(n);
	void *v = pak_bt_get_gatt_characteristic(s->bt, o->native, index);
	pointer_table_release(n);
	return add(s, BT_CHARACTERISTIC, v);
}

static int bt_char_op(wasm_exec_env_t e, uint32_t t, uint32_t h, int arg, int op) {
	struct bt_state *s = check(e, t);
	struct PtNode *n = acquire(s, h, BT_CHARACTERISTIC);
	if (!n)
		return -1;
	struct bt_object *o = pointer_table_payload(n);
	int r;
	if (op == 0)
		r = pak_bt_read_characteristic(s->bt, o->native, arg);
	else if (op == 1)
		r = pak_bt_set_watching_characteristic(s->bt, o->native, arg);
	else if (op == 2)
		r = pak_bt_watch_characteristic(s->bt, o->native, (unsigned)arg);
	else
		r = pak_bt_set_cccd(s->bt, o->native, arg);
	pointer_table_release(n);
	return r;
}

static int bt_read_char(wasm_exec_env_t e, uint32_t t, uint32_t h, int b) {
	return bt_char_op(e, t, h, b, 0);
}

static int bt_watching(wasm_exec_env_t e, uint32_t t, uint32_t h, int v) {
	return bt_char_op(e, t, h, v, 1);
}

static int bt_watch(wasm_exec_env_t e, uint32_t t, uint32_t h, int v) {
	return bt_char_op(e, t, h, v, 2);
}

static int bt_cccd(wasm_exec_env_t e, uint32_t t, uint32_t h, int v) {
	return bt_char_op(e, t, h, v, 3);
}

static int bt_write_char(wasm_exec_env_t e, uint32_t t, uint32_t h, uint32_t data, uint32_t len, int blocking) {
	struct bt_state *s = check(e, t);
	struct PtNode *n = acquire(s, h, BT_CHARACTERISTIC);
	if (!n)
		return -1;
	void *p = len ? wasm_guest_addr(s->inst, data, len) : NULL;
	if (len && (!data || !p)) {
		pointer_table_release(n);
		return -1;
	}
	struct bt_object *o = pointer_table_payload(n);
	int r = pak_bt_write_characteristic(s->bt, o->native, p, len, blocking);
	pointer_table_release(n);
	return r;
}

static uint32_t bt_cached(wasm_exec_env_t e, uint32_t t, uint32_t h, uint32_t data, uint32_t len) {
	struct bt_state *s = check(e, t);
	struct PtNode *n = acquire(s, h, BT_CHARACTERISTIC);
	if (!n)
		return 0;
	void *p = len ? wasm_guest_addr(s->inst, data, len) : NULL;
	if (len && (!data || !p)) {
		pointer_table_release(n);
		return 0;
	}
	struct bt_object *o = pointer_table_payload(n);
	uint32_t r = pak_bt_read_characteristic_cached_value(s->bt, o->native, p, len);
	pointer_table_release(n);
	return r;
}

static uint32_t bt_manufacturer(wasm_exec_env_t e, uint32_t t, uint32_t h, int index, uint32_t data, uint32_t len) {
	struct bt_state *s = check(e, t);
	struct PtNode *n = acquire(s, h, BT_DEVICE);
	if (!n)
		return 0;
	void *p = len ? wasm_guest_addr(s->inst, data, len) : NULL;
	if (len && (!data || !p)) {
		pointer_table_release(n);
		return 0;
	}
	struct bt_object *o = pointer_table_payload(n);
	uint32_t r = pak_bt_get_manufacturer_data(s->bt, o->native, index, p, len);
	pointer_table_release(n);
	return r;
}

/* RFCOMM is not part of the fake BLE backend yet. */
static uint32_t bt_socket(wasm_exec_env_t e, uint32_t t, uint32_t d, uint32_t uuid, int flags) {
	(void)e;
	(void)t;
	(void)d;
	(void)uuid;
	(void)flags;
	return 0;
}

static int bt_socket_io(wasm_exec_env_t e, uint32_t s, uint32_t data, uint32_t len) {
	(void)e;
	(void)s;
	(void)data;
	(void)len;
	return PAK_ERR_UNSUPPORTED;
}

static int bt_socket_close(wasm_exec_env_t e, uint32_t s) {
	(void)e;
	(void)s;
	return PAK_ERR_UNSUPPORTED;
}

#define SYM(name, fn, sig) {name, (void *)(uintptr_t)fn, sig, NULL}

bool wasm_bt_register(void) {
	static NativeSymbol syms[] = {
		SYM("pak_bt_get_context", bt_get_context, "()i"),
		SYM("pak_bt_unref_context", bt_unref_context, "(i)"),
		SYM("pak_bt_is_enabled", bt_enabled, "(i)i"),
		SYM("pak_bt_is_supported", bt_supported, "(ii)i"),
		SYM("pak_bt_get_n_adapters", bt_n_adapters, "(i)i"),
		SYM("pak_bt_get_adapter", bt_adapter, "(ii)i"),
		SYM("pak_bt_unref_adapter", bt_unref_adapter, "(ii)i"),
		SYM("pak_bt_get_n_devices", bt_n_devices, "(iii)i"),
		SYM("pak_bt_get_device", bt_device, "(iiii)i"),
		SYM("pak_bt_unref_device", bt_unref_device, "(ii)i"),
		SYM("pak_bt_device_update", bt_update, "(ii)i"),
		SYM("pak_bt_device_connect", bt_connect, "(iii)i"),
		SYM("pak_bt_device_disconnect", bt_disconnect, "(ii)i"),
		SYM("pak_bt_device_create_bond", bt_bond, "(iii)i"),
		SYM("pak_bt_set_device_callback", bt_callback, "(iiii)i"),
		SYM("pak_bt_get_manufacturer_data", bt_manufacturer, "(iiiii)i"),
		SYM("pak_bt_get_gatt_service", bt_service, "(iii)i"),
		SYM("pak_bt_unref_gatt_service", bt_unref_service, "(ii)i"),
		SYM("pak_bt_get_gatt_characteristic", bt_characteristic, "(iii)i"),
		SYM("pak_bt_unref_gatt_characteristic", bt_unref_characteristic, "(ii)i"),
		SYM("pak_bt_read_characteristic", bt_read_char, "(iii)i"),
		SYM("pak_bt_write_characteristic", bt_write_char, "(iiiii)i"),
		SYM("pak_bt_read_characteristic_cached_value", bt_cached, "(iiii)i"),
		SYM("pak_bt_set_watching_characteristic", bt_watching, "(iii)i"),
		SYM("pak_bt_watch_characteristic", bt_watch, "(iii)i"),
		SYM("pak_bt_set_cccd", bt_cccd, "(iii)i"),
		SYM("pak_bt_connect_to_service_channel", bt_socket, "(iiii)i"),
		SYM("pak_bt_read", bt_socket_io, "(iii)i"),
		SYM("pak_bt_write", bt_socket_io, "(iii)i"),
		SYM("pak_bt_close_socket", bt_socket_close, "(i)i"),
	};
	return wasm_runtime_register_natives("env", syms, sizeof(syms) / sizeof(syms[0]));
}
