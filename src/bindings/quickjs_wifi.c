#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <quickjs.h>
#include <quickjs-libc.h>
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
	struct PakNet *ctx;
	struct PakWiFiAdapter adapter;
};

static JSValue create_adapter(JSContext *ctx, JSValue wifi, struct PakNet *wifi_ctx, struct PakWiFiAdapter *adapter) {
	JSValue adapter_obj = JS_NewObjectClass(ctx, wifi_adapter_class_id);

	struct Adapter *adapter_priv = js_malloc_rt(JS_GetRuntime(ctx), sizeof(struct Adapter));
	adapter_priv->ctx = wifi_ctx;
	adapter_priv->adapter = *adapter;

	JS_SetOpaque(adapter_obj, adapter_priv);

	JS_SetPropertyStr(ctx, adapter_obj, "name", JS_NewString(ctx, adapter_priv->adapter.name));

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
	struct PakNet *wifi_ctx = JS_GetOpaque(this_val, wifi_class_id);
	struct PakWiFiAdapter pakadapter;
	pak_wifi_get_adapter(wifi_ctx, &pakadapter, -1);
	JSValue adapter = create_adapter(ctx, this_val, wifi_ctx, &pakadapter);
	return adapter;
}

static JSValue wifi_bind_to_adapter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[]) {
	struct PakNet *wifi_ctx = JS_GetOpaque(this_val, wifi_class_id);
	struct Adapter *adapter = JS_GetOpaque(argv[0], wifi_adapter_class_id);
	int32_t fd;
	JS_ToInt32(ctx, &fd, argv[1]);
	if (pak_wifi_bind_socket_to_adapter(wifi_ctx, &adapter->adapter, fd)) {
		return JS_ThrowInternalError(ctx, "pak_wifi_bind_socket_to_adapter");
	}
	return JS_UNDEFINED;
}

struct RequestConnectionPriv {
	struct PakNet *wifi_ctx;
	JSContext *ctx;
	JSValue this_val;
	JSValue fun_val;
};

static int connected(struct PakNet *ctx, struct PakWiFiAdapter *adapter, void *arg) {
	struct RequestConnectionPriv *temp_priv = arg;
	JSValue args[1] = {
		create_adapter(temp_priv->ctx, temp_priv->this_val, temp_priv->wifi_ctx, adapter),
	};
	JSValue rv = JS_Call(temp_priv->ctx, temp_priv->fun_val, JS_UNDEFINED, 1, args);
	if (JS_IsException(rv)) {
		printf("callback exception\n");
		JS_FreeValue(temp_priv->ctx, rv);
	}
	JS_FreeValue(temp_priv->ctx, args[0]);
	JS_FreeValue(temp_priv->ctx, temp_priv->fun_val);
	JS_FreeValue(temp_priv->ctx, temp_priv->this_val);
	free(temp_priv);
	return 0;
}

static JSValue wifi_request_connection(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[]) {
	struct PakNet *wifi_ctx = JS_GetOpaque(this_val, wifi_class_id);

	struct PakWiFiApFilter spec;
	JSValue ssid = JS_GetPropertyStr(ctx, argv[0], "ssidPattern");
	if (JS_IsUndefined(ssid)) {
		spec.has_ssid = 0;
	} else {
		spec.has_ssid = 1;
		strlcpy(spec.ssid_pattern, JS_ToCString(ctx, ssid), sizeof(spec.ssid_pattern));
	}
	JSValue password = JS_GetPropertyStr(ctx, argv[0], "password");
	if (JS_IsUndefined(password)) {
		spec.has_password = 0;
	} else {
		spec.has_password = 1;
		strlcpy(spec.password, JS_ToCString(ctx, password), sizeof(spec.password));
	}

	struct RequestConnectionPriv *temp_priv = malloc(sizeof(struct RequestConnectionPriv));
	temp_priv->ctx = ctx;
	temp_priv->wifi_ctx = wifi_ctx;
	temp_priv->fun_val = JS_DupValue(ctx, argv[1]);
	temp_priv->this_val = JS_DupValue(ctx, this_val);

	int rc = pak_wifi_request_connection(wifi_ctx, &spec, connected, temp_priv);
	if (rc) {
		JS_FreeValue(temp_priv->ctx, temp_priv->fun_val);
		JS_FreeValue(temp_priv->ctx, temp_priv->this_val);
		return JS_ThrowInternalError(ctx, "pak_wifi_request_connection: %d", rc);
	}

	JS_FreeValue(ctx, password);
	JS_FreeValue(ctx, ssid);
	
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry wifi_methods[] = {
	JS_CFUNC_DEF("getDefaultAdapter", 0, get_default_adapter),
	JS_CFUNC_DEF("bindSocketToAdapter", 2, wifi_bind_to_adapter),
	JS_CFUNC_DEF("requestConnection", 2, wifi_request_connection),

#define JS_CONSTANT(x) JS_PROP_INT32_DEF(#x, x, JS_PROP_CONFIGURABLE)
	JS_CONSTANT(PAK_WIFI_2GHZ),
	JS_CONSTANT(PAK_WIFI_2GHZ),
};

static JSValue js_wifi_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst argv[]) {
	JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	JSValue obj = JS_NewObjectProtoClass(ctx, proto, wifi_class_id);
	JS_FreeValue(ctx, proto);

	struct PakNet *wifictx = pak_net_get_context();

	JS_SetOpaque(obj, wifictx);
	
	return obj;
}

static void js_wifi_finalizer(JSRuntime *rt, JSValue val) {
}

static int module_wifi(JSContext *ctx, JSModuleDef *m) {
	const char *class_name = "WiFi";

	const JSClassDef js_class = {
		.class_name = class_name,
		.finalizer = js_wifi_finalizer,
	};

	JS_NewClassID(&wifi_class_id);
	JS_NewClass(JS_GetRuntime(ctx), wifi_class_id, &js_class);

	JSValue proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, proto, wifi_methods, sizeof(wifi_methods) / sizeof(wifi_methods[0]));
	JS_SetClassProto(ctx, wifi_class_id, proto);

	JSValue class = JS_NewCFunction2(ctx, js_wifi_constructor, class_name, 5, JS_CFUNC_constructor, 0);
	JS_SetConstructor(ctx, class, proto);

	JS_SetModuleExport(ctx, m, class_name, class);
	return 0;
}

static int module_wifi_all(JSContext *ctx, JSModuleDef *m) {
	module_wifi(ctx, m);
	module_wifi_adapter(ctx, m);
	return 0;
}

JSModuleDef *js_init_module_wifi(JSContext *ctx, const char *module_name) {
	JSModuleDef *m = JS_NewCModule(ctx, module_name, module_wifi_all);
	if (!m) return NULL;
	return m;
}
