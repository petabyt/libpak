#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>
#include <wifi.h>

__attribute__((weak))
int JS_GetLength(JSContext *ctx, JSValueConst obj, int64_t *pres) {
	JSValue length = JS_GetPropertyStr(ctx, obj, "length");
	if (JS_IsNumber(length)) {
		JS_ToInt64(ctx, pres, length);
		return 0;
	}
	JS_FreeValue(ctx, length);
	return -1;
}

static JSClassID wifi_class_id = 0;
static JSClassID wifi_adapter_class_id = 0;

struct Adapter {
	struct PakWiFi *ctx;
	struct PakWiFiAdapter adapter;
};

static JSValue create_adapter(JSContext *ctx, JSValue wifi, struct PakWiFi *wifi_ctx, int index) {
	JSValue adapter_obj = JS_NewObjectClass(ctx, wifi_adapter_class_id);

	struct Adapter *adapter_priv = js_malloc_rt(JS_GetRuntime(ctx), sizeof(struct Adapter));
	adapter_priv->ctx = wifi_ctx;
	pak_wifi_get_adapter(wifi_ctx, &adapter_priv->adapter, index);

	JS_SetOpaque(adapter_obj, adapter_priv);

	JS_SetPropertyStr(ctx, adapter_obj, "parent", JS_DupValue(ctx, wifi));
	return adapter_obj;
}

static void adapter_finalizer(JSRuntime *rt, JSValue val) {
	struct Adapter *adapter = JS_GetOpaque(val, wifi_adapter_class_id);
	pak_wifi_unref_adapter(adapter->ctx, &adapter->adapter);
}

static int module_wifi_adapter(JSContext* ctx, JSModuleDef *m) {
	const char *class_name = "WiFiAdapter";

	const JSClassDef js_class = {
		.class_name = class_name,
		.finalizer = adapter_finalizer,
	};

	JS_NewClassID(&wifi_adapter_class_id);
	JS_NewClass(JS_GetRuntime(ctx), wifi_adapter_class_id, &js_class);

	JSValue proto = JS_NewObject(ctx);
	JS_SetClassProto(ctx, wifi_adapter_class_id, proto);

	JSValue class = JS_NewObject(ctx);

	JS_SetModuleExport(ctx, m, class_name, class);
	return 0;
}

static JSValue get_default_adapter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[]) {
	struct PakWiFi *wifi_ctx = JS_GetOpaque(this_val, wifi_class_id);
	JSValue adapter = create_adapter(ctx, this_val, wifi_ctx, -1);
	return adapter;
}

static const JSCFunctionListEntry wifi_methods[] = {
	JS_CFUNC_DEF("getDefaultAdapter", 0, get_default_adapter),
	//JS_CFUNC_DEF("bindSocketToAdapter", 0, get_default_adapter),

	JS_PROP_INT32_DEF("WIFI_2GHZ",		   1, JS_PROP_ENUMERABLE),
	JS_PROP_INT32_DEF("WIFI_5GHZ",		   2, JS_PROP_ENUMERABLE),
};

static JSValue js_directory_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst argv[]) {
	JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	JSValue obj = JS_NewObjectProtoClass(ctx, proto, wifi_class_id);
	JS_FreeValue(ctx, proto);

	struct PakWiFi *wifictx = pak_wifi_get_context();

	JS_SetOpaque(obj, wifictx);
	
	return obj;
}

static void js_directory_finalizer(JSRuntime *rt, JSValue val) {
}

static int module_wifi(JSContext* ctx, JSModuleDef *m) {
	const char *class_name = "WiFi";

	const JSClassDef js_class = {
		.class_name = class_name,
		.finalizer = js_directory_finalizer,
	};

	JS_NewClassID(&wifi_class_id);
	JS_NewClass(JS_GetRuntime(ctx), wifi_class_id, &js_class);

	JSValue proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, proto, wifi_methods, sizeof(wifi_methods) / sizeof(wifi_methods[0]));
	JS_SetClassProto(ctx, wifi_class_id, proto);

	JSValue class = JS_NewCFunction2(ctx, js_directory_constructor, class_name, 5, JS_CFUNC_constructor, 0);
	JS_SetConstructor(ctx, class, proto);

	JS_SetModuleExport(ctx, m, class_name, class);
	return 0;
}

static int module_wifi_all(JSContext* ctx, JSModuleDef *m) {
	module_wifi(ctx, m);
	module_wifi_adapter(ctx, m);
	return 0;
}

JSModuleDef *js_init_module_wifi(JSContext *ctx, const char *module_name) {
	JSModuleDef *m = JS_NewCModule(ctx, module_name, module_wifi_all);
	if (!m) return NULL;
	return m;
}
