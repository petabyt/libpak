#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <quickjs.h>
#include <quickjs-libc.h>
#include <bluetooth.h>

static JSClassID bt_class_id = 0;
static JSClassID bluetooth_adapter_class_id = 0;
static JSClassID bluetooth_device_class_id = 0;
static JSClassID bluetooth_socket_class_id = 0;
static JSClassID bluetooth_service_class_id = 0;
static JSClassID bluetooth_char_class_id = 0;
static JSClassID bluetooth_desc_class_id = 0;

static int register_class_helper(JSContext* ctx, JSModuleDef *m, const char *name, JSCFunction *ctor, JSClassFinalizer *finalizer, JSClassID *class_id, const JSCFunctionListEntry *methods) {
	const JSClassDef js_class = {
		.class_name = name,
		.finalizer = finalizer,
	};
	int cnt;
	for (cnt = 0; methods[cnt].name != NULL; cnt++);
	JS_NewClassID(class_id);
	JS_NewClass(JS_GetRuntime(ctx), *class_id, &js_class);
	JSValue proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, proto, methods, cnt);
	JS_SetClassProto(ctx, *class_id, proto);
	if (ctor == NULL) {
		JSValue class = JS_NewCFunction2(ctx, ctor, name, 0, JS_CFUNC_constructor, 0);
		JS_SetConstructor(ctx, class, proto);
	} else {
		JSValue class = JS_NewObject(ctx);
		JS_SetModuleExport(ctx, m, name, class);
	}

	return 0;
}

static JSValue helper_dummy_promise(JSContext *ctx, int rc, JSValue return_value) {
	JSValue resolving_funcs[2];
	JSValue val = JS_NewPromiseCapability(ctx, resolving_funcs);
	if (rc) {
		JSValue arg[] = { JS_NewError(ctx) };
		JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, arg);
	} else {
		JSValue arg[] = { return_value };
		JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, arg);
	}
	JS_FreeValue(ctx, resolving_funcs[0]);
	JS_FreeValue(ctx, resolving_funcs[1]);
	return val;
}

struct CharWrapper {
	struct PakBt *ctx;
	struct PakGattCharacteristic *c;
};
static JSValue pak_js_create_char(JSContext *ctx, struct PakBt *bt, struct PakGattCharacteristic *c) {
	JSValue obj = JS_NewObjectClass(ctx, (int)bluetooth_char_class_id);
	struct CharWrapper *priv = js_malloc_rt(JS_GetRuntime(ctx), sizeof(struct CharWrapper));
	priv->ctx = bt;
	priv->c = c;
	JS_SetOpaque(obj, priv);
	return obj;
}
static void char_finalizer(JSRuntime *rt, JSValue val) {
	struct CharWrapper *wrapper = JS_GetOpaque(val, bluetooth_adapter_class_id);
	pak_bt_unref_gatt_characteristic(wrapper->ctx, wrapper->c);
}
enum CharMethods {
	M_SET_VALUE,
	M_GET_VALUE,
};
static JSValue magic_char_method(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[], int magic) {
	struct CharWrapper *wrapper = JS_GetOpaque(this_val, bluetooth_char_class_id);
	switch (magic) {
		case M_SET_VALUE: {
			size_t len;
			void *buf = JS_GetArrayBuffer(ctx, &len, argv[0]);
			int rc = pak_bt_write_characteristic(wrapper->ctx, wrapper->c, buf, len, PAK_BT_BLOCK);
			return helper_dummy_promise(ctx, rc, JS_UNDEFINED);
		}
		case M_GET_VALUE: {
			int rc = pak_bt_read_characteristic(wrapper->ctx, wrapper->c, PAK_BT_BLOCK);
			uint8_t buffer[512];
			unsigned int sz = pak_bt_read_characteristic_cached_value(wrapper->ctx, wrapper->c, buffer, sizeof(buffer));
			return helper_dummy_promise(ctx, rc, JS_NewArrayBufferCopy(ctx, buffer, sz));
		}
	}
	return JS_NULL;
}
static const JSCFunctionListEntry char_methods[] = {
	JS_CFUNC_DEF("getDescriptors", 2, NULL),
	JS_CFUNC_DEF("getDescriptor", 2, NULL),
	JS_CFUNC_MAGIC_DEF("getValue", 2, magic_char_method, M_GET_VALUE),
	JS_CFUNC_MAGIC_DEF("setValue", 2, magic_char_method, M_SET_VALUE),
	{NULL},
};

struct ServiceWrapper {
	struct PakBt *ctx;
	struct PakGattService *s;
};
static JSValue pak_js_create_service(JSContext *ctx, JSValue bt, struct PakGattService *s) {
	JSValue obj = JS_NewObjectClass(ctx, (int)bluetooth_service_class_id);
	struct ServiceWrapper *priv = js_malloc_rt(JS_GetRuntime(ctx), sizeof(struct ServiceWrapper));
	priv->ctx = (struct PakBt *)JS_GetOpaque(bt, bt_class_id);
	priv->s = s;
	JS_SetOpaque(obj, priv);
	return obj;
}
static void service_finalizer(JSRuntime *rt, JSValue val) {
	struct ServiceWrapper *wrapper = JS_GetOpaque(val, bluetooth_adapter_class_id);
	pak_bt_unref_gatt_service(wrapper->ctx, wrapper->s);
}
enum SvcMethods {
	M_GET_CHAR,
	M_GET_CHARS,
};
static JSValue magic_svc_method(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[], int magic) {
	struct ServiceWrapper *wrapper = JS_GetOpaque(this_val, bluetooth_service_class_id);
	switch (magic) {
		case M_GET_CHAR: {
			struct PakGattCharacteristic *chr = pak_bt_get_gatt_characteristic_uuid(wrapper->ctx, wrapper->s, JS_ToCString(ctx, argv[0]));
			return pak_js_create_char(ctx, wrapper->ctx, chr);
		}
		case M_GET_CHARS: return JS_NewError(ctx); // TODO
	}
	return JS_NULL;
}
static const JSCFunctionListEntry service_methods[] = {
	JS_CFUNC_MAGIC_DEF("getCharacteristics", 0, magic_svc_method, M_GET_CHARS),
	JS_CFUNC_MAGIC_DEF("getCharacteristic", 1, magic_svc_method, M_GET_CHAR),
	{NULL},
};

struct SocketWrapper {
	struct PakBt *ctx;
	struct PakBtSocket *s;
};
static JSValue pak_js_create_socket(JSContext *ctx, JSValue bt, struct PakBtSocket *s) {
	JSValue obj = JS_NewObjectClass(ctx, (int)bluetooth_socket_class_id);
	struct SocketWrapper *priv = js_malloc_rt(JS_GetRuntime(ctx), sizeof(struct SocketWrapper));
	priv->ctx = (struct PakBt *)JS_GetOpaque(bt, bt_class_id);
	priv->s = s;
	JS_SetOpaque(obj, priv);
	return obj;
}
static void socket_finalizer(JSRuntime *rt, JSValue val) {
	struct SocketWrapper *wrapper = JS_GetOpaque(val, bluetooth_adapter_class_id);
	pak_bt_close_socket(wrapper->s);
}
enum SocketMethods {
	M_READ,
	M_WRITE,
	M_CLOSE,
};
static JSValue magic_socket_method(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[], int magic) {
#define min(a, b)  ((a) < (b) ? (a) : (b))
	struct SocketWrapper *wrapper = JS_GetOpaque(this_val, bluetooth_socket_class_id);
	switch (magic) {
		case M_READ: {
			size_t len;
			int32_t len2;
			void *buf = JS_GetArrayBuffer(ctx, &len, argv[1]);
			JS_ToInt32(ctx, &len2, argv[0]);
			return JS_NewInt32(ctx, pak_bt_read(wrapper->s, buf, min(len2, len)));
		}
		case M_WRITE: {
			size_t len;
			int32_t len2;
			void *buf = JS_GetArrayBuffer(ctx, &len, argv[1]);
			JS_ToInt32(ctx, &len2, argv[0]);
			return JS_NewInt32(ctx, pak_bt_write(wrapper->s, buf, min(len2, len)));
		}
		case M_CLOSE: return JS_NewInt32(ctx, pak_bt_close_socket(wrapper->s));
	}
	return JS_NULL;
}
static const JSCFunctionListEntry socket_methods[] = {
	JS_CFUNC_MAGIC_DEF("read", 2, magic_socket_method, M_READ),
	JS_CFUNC_MAGIC_DEF("write", 2, magic_socket_method, M_WRITE),
	JS_CFUNC_MAGIC_DEF("close", 0, magic_socket_method, M_CLOSE),
	{NULL},
};

struct Device {
	struct PakBt *ctx;
	struct PakBtDevice *dev;
};
JSValue pak_js_create_device(JSContext *ctx, struct PakBt *bt, struct PakBtDevice *dev) {
	JSValue obj = JS_NewObjectClass(ctx, (int)bluetooth_device_class_id);
	JS_SetPropertyStr(ctx, obj, "name", JS_NewString(ctx, dev->name));
	JS_SetPropertyStr(ctx, obj, "macAddress", JS_NewString(ctx, dev->mac_address));
	struct Device *priv = js_malloc_rt(JS_GetRuntime(ctx), sizeof(struct Device));
	priv->ctx = bt;
	priv->dev = dev;
	JS_SetOpaque(obj, priv);
	return obj;
}
static void device_finalizer(JSRuntime *rt, JSValue val) {
	struct Device *wrapper = JS_GetOpaque(val, bluetooth_device_class_id);
	pak_bt_unref_device(wrapper->ctx, wrapper->dev);
}
enum DeviceMethodMagic {
	M_IS_CLASSIC,
	M_IS_CONNECTED,
	M_IS_BONDED,
	M_CONNECT,
	M_DISCONNECT,
	M_CREATE_BOND,
	M_GET_MFG_DATA,
	M_CONNECT_SVC_CHANNEL,
	M_GET_GATT_SERVICE,
	M_GET_GATT_SERVICES,
};
static JSValue magic_device_method(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[], int magic) {
	struct Device *dev = JS_GetOpaque(this_val, bluetooth_device_class_id);
	switch (magic) {
		case M_IS_CLASSIC: pak_bt_device_update(dev->ctx, dev->dev); return JS_NewBool(ctx, dev->dev->is_classic);
		case M_IS_BONDED: pak_bt_device_update(dev->ctx, dev->dev); return JS_NewBool(ctx, dev->dev->is_bonded);
		case M_IS_CONNECTED: pak_bt_device_update(dev->ctx, dev->dev); return JS_NewBool(ctx, dev->dev->is_connected);
		case M_CONNECT: return helper_dummy_promise(ctx, pak_bt_device_connect(dev->ctx, dev->dev), JS_NULL);
	}
	return JS_NULL;
}
static const JSCFunctionListEntry device_methods[] = {
	JS_CFUNC_MAGIC_DEF("isClassic", 0, magic_device_method, M_IS_CLASSIC),
	JS_CFUNC_MAGIC_DEF("isConnected", 0, magic_device_method, M_IS_CONNECTED),
	JS_CFUNC_MAGIC_DEF("isBonded", 0, magic_device_method, M_IS_BONDED),
	JS_CFUNC_MAGIC_DEF("connect", 0, magic_device_method, M_CONNECT),
	JS_CFUNC_MAGIC_DEF("disconnect", 0, NULL, M_DISCONNECT),
	JS_CFUNC_MAGIC_DEF("createBond", 0, NULL, M_CREATE_BOND),
	JS_CFUNC_MAGIC_DEF("getManufacturerData", 1, NULL, M_GET_MFG_DATA),
	JS_CFUNC_MAGIC_DEF("connectToServiceChannel", 1, NULL, M_CONNECT_SVC_CHANNEL),
	JS_CFUNC_MAGIC_DEF("getGattServices", 0, NULL, M_GET_GATT_SERVICE),
	JS_CFUNC_MAGIC_DEF("getGattService", 1, NULL, M_GET_GATT_SERVICES),
	{NULL},
};

struct Adapter {
	struct PakBt *ctx;
	struct PakBtAdapter *adapter;
};
static JSValue pak_js_create_adapter(JSContext *ctx, JSValue bt, struct PakBtAdapter *adapter) {
	JSValue adapter_obj = JS_NewObjectClass(ctx, (int)bluetooth_adapter_class_id);

	struct Adapter *adapter_priv = js_malloc_rt(JS_GetRuntime(ctx), sizeof(struct Adapter));
	adapter_priv->ctx = (struct PakBt *)JS_GetOpaque(bt, bt_class_id);
	adapter_priv->adapter = adapter;

	JS_SetOpaque(adapter_obj, adapter_priv);
	return adapter_obj;
}
static void adapter_finalizer(JSRuntime *rt, JSValue val) {
	struct Adapter *adapter = JS_GetOpaque(val, bluetooth_adapter_class_id);
	pak_bt_unref_adapter(adapter->ctx, adapter->adapter);
	js_free_rt(rt, adapter);
}
static JSValue get_devices(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[]) {
	struct Adapter *bt_adapter = JS_GetOpaque(this_val, bluetooth_adapter_class_id);

	JSValue arr = JS_NewArray(ctx);
	int i = 0;
	while (1) {
		struct PakBtDevice *dev = pak_bt_get_device(bt_adapter->ctx, bt_adapter->adapter, i, PAK_FILTER_BONDED);
		if (dev == NULL) break;
		JS_SetPropertyUint32(ctx, arr, i, pak_js_create_device(ctx, bt_adapter->ctx, dev));
		i++;
	}

	return arr;
}
static const JSCFunctionListEntry adapter_methods[] = {
	JS_CFUNC_DEF("getDevices", 0, get_devices),
	{NULL},
};

static JSValue get_adapters(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[]) { abort(); }
static JSValue get_default_adapter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[]) {
	struct PakBt *bt_ctx = JS_GetOpaque(this_val, bt_class_id);
	struct PakBtAdapter *pakadapter = pak_bt_get_adapter(bt_ctx, -1);
	JSValue adapter = pak_js_create_adapter(ctx, this_val, pakadapter);
	return adapter;
}
static JSValue js_bt_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst argv[]) {
	JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	JSValue obj = JS_NewObjectProtoClass(ctx, proto, bt_class_id);
	JS_FreeValue(ctx, proto);
	JS_SetOpaque(obj, pak_bt_get_context());
	return obj;
}
static void js_bt_finalizer(JSRuntime *rt, JSValue val) {}
static const JSCFunctionListEntry bluetooth_methods[] = {
	JS_CFUNC_DEF("getDefaultAdapter", 0, get_default_adapter),
	JS_CFUNC_DEF("getAdapters", 0, get_adapters),
	{NULL},
};

JSValue pak_js_create_bt_context(JSContext *ctx, struct PakBt *bt_ctx) {
	JSValue ctx_obj = JS_NewObjectClass(ctx, (int)bt_class_id);
	JS_SetOpaque(ctx_obj, bt_ctx);
	return ctx_obj;
}

static int module_bt_all(JSContext *ctx, JSModuleDef *m) {
	register_class_helper(ctx, m, "Bluetooth", js_bt_constructor, js_bt_finalizer, &bt_class_id, bluetooth_methods);
	register_class_helper(ctx, m, "BluetoothAdapter", NULL, adapter_finalizer, &bluetooth_adapter_class_id, adapter_methods);
	register_class_helper(ctx, m, "BluetoothDevice", NULL, device_finalizer, &bluetooth_device_class_id, device_methods);
	register_class_helper(ctx, m, "BluetoothSocket", NULL, socket_finalizer, &bluetooth_socket_class_id, socket_methods);
	register_class_helper(ctx, m, "GattService", NULL, service_finalizer, &bluetooth_service_class_id, service_methods);
	register_class_helper(ctx, m, "GattCharacteristic", NULL, char_finalizer, &bluetooth_char_class_id, char_methods);
	return 0;
}

JSModuleDef *js_init_module_bluetooth(JSContext *ctx, const char *module_name) {
	return JS_NewCModule(ctx, module_name, module_bt_all);
}
