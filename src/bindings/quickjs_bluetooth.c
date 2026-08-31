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

struct Adapter {
	struct PakBt *ctx;
	struct PakBtAdapter *adapter;
};
static JSValue pak_js_create_adapter(JSContext *ctx, JSValue bt, struct PakBtAdapter *adapter) {
	JSValue adapter_obj = JS_NewObjectClass(ctx, bluetooth_adapter_class_id);

	struct Adapter *adapter_priv = js_malloc_rt(JS_GetRuntime(ctx), sizeof(struct Adapter));
	adapter_priv->ctx = (struct PakBt *)JS_GetOpaque(bt, bt_class_id);
	adapter_priv->adapter = adapter;

	JS_SetOpaque(adapter_obj, adapter_priv);
	return adapter_obj;
}
static void adapter_finalizer(JSRuntime *rt, JSValue val) {
	struct Adapter *adapter = JS_GetOpaque(val, bluetooth_adapter_class_id);
	pak_bt_unref_adapter(adapter->ctx, adapter->adapter);
}
static int module_bluetooth_adapter(JSContext* ctx, JSModuleDef *m) {
	const char *class_name = "BluetoothAdapter";
	const JSClassDef js_class = {
		.class_name = class_name,
		.finalizer = adapter_finalizer,
	};
	static const JSCFunctionListEntry methods[] = {
		JS_CFUNC_DEF("getDevices", 0, NULL), // TODO
	};
	JS_NewClassID(&bluetooth_adapter_class_id);
	JS_NewClass(JS_GetRuntime(ctx), bluetooth_adapter_class_id, &js_class);
	JSValue proto = JS_NewObject(ctx);
	JS_SetClassProto(ctx, bluetooth_adapter_class_id, proto);
	JSValue class = JS_NewObject(ctx);
	JS_SetModuleExport(ctx, m, class_name, class);
	return 0;
}

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
static int module_bluetooth(JSContext *ctx, JSModuleDef *m) {
	const char *class_name = "Bluetooth";
	const JSClassDef js_class = {
			.class_name = class_name,
			.finalizer = js_bt_finalizer,
		};
	static const JSCFunctionListEntry methods[] = {
		JS_CFUNC_DEF("getDefaultAdapter", 0, get_default_adapter),
	};

	JS_NewClassID(&bt_class_id);
	JS_NewClass(JS_GetRuntime(ctx), bt_class_id, &js_class);

	JSValue proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, proto, methods, sizeof(methods) / sizeof(methods[0]));
	JS_SetClassProto(ctx, bt_class_id, proto);

	JSValue class = JS_NewCFunction2(ctx, js_bt_constructor, class_name, 5, JS_CFUNC_constructor, 0);
	JS_SetConstructor(ctx, class, proto);

	JS_SetModuleExport(ctx, m, class_name, class);
	return 0;
}

JSValue pak_js_create_bt_context(JSContext *ctx, struct PakBt *bt_ctx) {
	JSValue ctx_obj = JS_NewObjectClass(ctx, bt_class_id);
	JS_SetOpaque(ctx_obj, bt_ctx);
	return ctx_obj;
}

static int module_bt_all(JSContext *ctx, JSModuleDef *m) {
	module_bluetooth(ctx, m);
	module_bluetooth_adapter(ctx, m);
	return 0;
}

JSModuleDef *js_init_module_bluetooth(JSContext *ctx, const char *module_name) {
	JSModuleDef *m = JS_NewCModule(ctx, module_name, module_bt_all);
	if (!m) return NULL;
	return m;
}
