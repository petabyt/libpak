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

static JSValue test_module(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[]) {
	JSValue obj = argv[0];
	struct Module *mod = JS_GetOpaque(obj, module_class_id);	

	mod->on_find_connection(mod, -1);
	mod->on_run_test(mod, SCREEN_TEST_SUITE, -1);
	mod->on_disconnect(mod);

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("test", 1, test_module),
};

static const JSCFunctionListEntry module_methods[] = {
	JS_CFUNC_MAGIC_DEF("enterScreen", 1, generic_operation, M_ENTER_SCREEN),
	JS_CFUNC_MAGIC_DEF("enterCustomScreen", 3, generic_operation, M_ENTER_CUSTOM_SCREEN),

#define JS_CONSTANT(x) JS_PROP_INT32_DEF(#x, x, JS_PROP_CONFIGURABLE)
	JS_CONSTANT(SCREEN_CONNECT_WIFI),
	JS_CONSTANT(SCREEN_CONNECT_USB),

};

static int init(struct Module *mod) { return 0; }

static int on_try_connect_wifi(struct Module *mod, struct PakWiFiAdapter *handle, int job) {
//	JSValue fun = JS_GetPropertyStr(mod->priv->ctx, mod->priv->object, "onTryConnectWiFi");
//	JSValue args[1] = {
//		JS_NewInt32(mod->priv->ctx, job),
//	};
//	JS_Call(mod->priv->ctx, fun, mod->priv->object, 1, args);
//	JS_FreeValue(mod->priv->ctx, args[0]);
//	JS_FreeValue(mod->priv->ctx, fun);
	return 0;
}

static int on_find_connection(struct Module *mod, int job) {
	JSValue fun = JS_GetPropertyStr(mod->priv->ctx, mod->priv->object, "onFindConnection");
	JSValue args[1] = {
		JS_NewInt32(mod->priv->ctx, job),
	};
	JS_Call(mod->priv->ctx, fun, mod->priv->object, 1, args);
	JS_FreeValue(mod->priv->ctx, args[0]);
	JS_FreeValue(mod->priv->ctx, fun);
	return 0;
}

static int on_run_test(struct Module *mod, int screen, int job) {
	JSValue fun = JS_GetPropertyStr(mod->priv->ctx, mod->priv->object, "onRunTest");
	JSValue args[2] = {
		JS_NewInt32(mod->priv->ctx, screen),
		JS_NewInt32(mod->priv->ctx, job),
	};
	JS_Call(mod->priv->ctx, fun, mod->priv->object, 2, args);
	JS_FreeValue(mod->priv->ctx, args[0]);
	JS_FreeValue(mod->priv->ctx, args[1]);
	JS_FreeValue(mod->priv->ctx, fun);
	return 0;
}

static int on_disconnect(struct Module *mod) {
	JSValue fun = JS_GetPropertyStr(mod->priv->ctx, mod->priv->object, "onDisconnect");
	JS_Call(mod->priv->ctx, fun, mod->priv->object, 0, NULL);
	JS_FreeValue(mod->priv->ctx, fun);
	return 0;
}

static JSValue js_module_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst argv[]) {
	JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	JSValue obj = JS_NewObjectProtoClass(ctx, proto, module_class_id);
	JS_FreeValue(ctx, proto);

	struct Module *mod = pak_create_mod();
	mod->init = init;
	mod->on_run_test = on_run_test;
	mod->on_try_connect_wifi = on_try_connect_wifi;
	mod->on_find_connection = on_find_connection;
	mod->on_disconnect = on_disconnect;
	mod->priv = malloc(sizeof(struct ModulePriv));
	mod->priv->object = obj;
	mod->priv->ctx = ctx;
	mod->priv->ctx = ctx;

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
	JS_SetPropertyFunctionList(ctx, proto, module_methods, sizeof(module_methods) / sizeof(module_methods[0]));
	JS_SetClassProto(ctx, module_class_id, proto);

	JSValue class = JS_NewCFunction2(ctx, js_module_constructor, class_name, 5, JS_CFUNC_constructor, 0);
	JS_SetPropertyFunctionList(ctx, class, module_funcs, sizeof(module_funcs) / sizeof(module_funcs[0]));
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
