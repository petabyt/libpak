#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>
#include <runtime.h>

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

static JSClassID module_class_id = 0;

struct ModulePriv {
	JSContext *ctx;
	JSRuntime *rt;
	JSValue object;
};

static int connected(struct PakNet *ctx, struct PakWiFiAdapter *adapter, void *arg) {
}

static int manual_connect(struct Module *mod) {
}

static int init(struct Module *mod) {
}

static int on_try_connect_wifi(struct Module *mod, struct PakWiFiAdapter *handle, int job) {
	return 0;
}

static JSValue generic_operation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[], int magic) {
	struct Module *mod = (struct Module *)JS_GetOpaque(this_val, module_class_id);
	int32_t fd;
	JS_ToInt32(ctx, &fd, argv[0]);
	return JS_UNDEFINED;
}

enum Operations {
	M_ENTER_SCREEN,
	M_ENTER_CUSTOM_SCREEN,
	M_SET_PROGRESS_BAR,
	M_SET_CURRENT_DOWNLOAD_SPEED,
	M_SET_DEVICE_NAME,
};

static const JSCFunctionListEntry wifi_methods[] = {
	JS_CFUNC_MAGIC_DEF("enterScreen", 1, generic_operation, M_ENTER_SCREEN),
	JS_CFUNC_MAGIC_DEF("enterCustomScreen", 3, generic_operation, M_ENTER_SCREEN),

#define JS_CONSTANT(x) JS_PROP_INT32_DEF(#x, x, JS_PROP_CONFIGURABLE)
	JS_CONSTANT(SCREEN_CONNECT_WIFI),
	JS_CONSTANT(SCREEN_CONNECT_USB),

};

static JSValue js_module_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst argv[]) {
	JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	JSValue obj = JS_NewObjectProtoClass(ctx, proto, module_class_id);
	JS_FreeValue(ctx, proto);

	struct Module *mod = malloc(sizeof(struct Module));
	mod->priv = malloc(sizeof(struct ModulePriv));

	JS_SetOpaque(obj, mod);
	
	return obj;
}

static void js_wifi_finalizer(JSRuntime *rt, JSValue val) {
}

static int module_module(JSContext *ctx, JSModuleDef *m) {
	const char *class_name = "Module";

	const JSClassDef js_class = {
		.class_name = class_name,
		.finalizer = js_wifi_finalizer,
	};

	JS_NewClassID(&module_class_id);
	JS_NewClass(JS_GetRuntime(ctx), module_class_id, &js_class);

	JSValue proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, proto, wifi_methods, sizeof(wifi_methods) / sizeof(wifi_methods[0]));
	JS_SetClassProto(ctx, module_class_id, proto);

	JSValue class = JS_NewCFunction2(ctx, js_module_constructor, class_name, 5, JS_CFUNC_constructor, 0);
	JS_SetConstructor(ctx, class, proto);

	JS_SetModuleExport(ctx, m, class_name, class);
	return 0;
}

static int module_wifi_all(JSContext *ctx, JSModuleDef *m) {
	module_module(ctx, m);
	return 0;
}

JSModuleDef *js_init_module_pak_runtime(JSContext *ctx, const char *module_name) {
	JSModuleDef *m = JS_NewCModule(ctx, module_name, module_wifi_all);
	if (!m) return NULL;
	return m;
}
