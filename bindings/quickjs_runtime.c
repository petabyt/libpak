#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <quickjs.h>
#include <quickjs-libc.h>
#include <runtime.h>

JSModuleDef *js_init_module_socket(JSContext *ctx, const char *module_name);
JSModuleDef *js_init_module_wifi(JSContext *ctx, const char *module_name);

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

	if (pak_rt_test_module(mod)) {
		return JS_UNDEFINED;
	}

	return JS_UNDEFINED;
}

static JSValue export_module(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[]) {
	JSValue obj = argv[0];
	struct Module *mod = JS_GetOpaque(obj, module_class_id);	

	JS_SetContextOpaque(ctx, mod);

	return JS_UNDEFINED;
}


static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("test", 1, test_module),
	JS_CFUNC_DEF("export", 1, export_module),
};

static const JSCFunctionListEntry module_methods[] = {
	JS_CFUNC_MAGIC_DEF("enterScreen", 1, generic_operation, M_ENTER_SCREEN),
	JS_CFUNC_MAGIC_DEF("enterCustomScreen", 3, generic_operation, M_ENTER_CUSTOM_SCREEN),

#define JS_CONSTANT(x) JS_PROP_INT32_DEF(#x, x, JS_PROP_CONFIGURABLE)
	JS_CONSTANT(SCREEN_CONNECT_WIFI),
	JS_CONSTANT(SCREEN_CONNECT_USB),

};

static int init(struct Module *mod) { return 0; }

static int call_module_method(struct JSContext *ctx, JSValue obj, const char *name, int argc, JSValue *argv) {
	int rc = 0;
	JSValue fun = JS_GetPropertyStr(ctx, obj, name);
	JSValue rv = JS_Call(ctx, fun, obj, argc, argv);
	if (JS_IsException(rv)) {
		const char *str = JS_ToCString(ctx, rv);
		printf("%s: %s\n", name, str);
		JS_FreeCString(ctx, str);
		JS_FreeValue(ctx, rv);
		rc = -1;
	}
	for (int i = 0; i < argc; i++) {
		JS_FreeValue(ctx, argv[i]);
	}
	JS_FreeValue(ctx, fun);
	return rc;
}

static int on_try_connect_wifi(struct Module *mod, struct PakWiFiAdapter *handle, int job) {
	JSValue args[] = {
		JS_UNDEFINED,
		JS_UNDEFINED,
		JS_NewInt32(mod->priv->ctx, job),
	};
	return call_module_method(mod->priv->ctx, mod->priv->object, "onTryConnectWiFi", 2, args);
}

static int on_find_connection(struct Module *mod, int job) {
	JSValue args[1] = {
		JS_NewInt32(mod->priv->ctx, job),
	};
	return call_module_method(mod->priv->ctx, mod->priv->object, "onFindConnection", 0, args);
}

static int on_run_test(struct Module *mod, int screen, int job) {
	JSValue args[2] = {
		JS_NewInt32(mod->priv->ctx, screen),
		JS_NewInt32(mod->priv->ctx, job),
	};
	return call_module_method(mod->priv->ctx, mod->priv->object, "onRunTest", 2, args);
}

static int on_disconnect(struct Module *mod) {
	return call_module_method(mod->priv->ctx, mod->priv->object, "onDisconnect", 0, NULL);
}

static JSValue js_module_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst argv[]) {
	JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	JSValue obj = JS_NewObjectProtoClass(ctx, proto, module_class_id);
	JS_FreeValue(ctx, proto);

	struct Module *mod = calloc(1, sizeof(struct Module));
	mod->init = init;
	mod->on_run_test = on_run_test;
	mod->on_try_connect_wifi = on_try_connect_wifi;
	mod->on_find_connection = on_find_connection;
	mod->on_disconnect = on_disconnect;

	mod->priv = malloc(sizeof(struct ModulePriv));
	mod->priv->object = obj;
	mod->priv->ctx = ctx;

	JS_SetOpaque(obj, mod);
	
	return obj;
}

static void js_module_finalizer(JSRuntime *rt, JSValue val) {
}

static int module_module(JSContext *ctx, JSModuleDef *m) {
	const char *class_name = "Module";

	const JSClassDef js_class = {
		.class_name = class_name,
		.finalizer = js_module_finalizer,
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

int free_module_and_runtime(struct Module *mod) {
	js_std_free_handlers(mod->priv->rt);
	JS_FreeContext(mod->priv->ctx);
	JS_FreeRuntime(mod->priv->rt);
}

static JSModuleDef *my_module_loader(JSContext *ctx, const char *module_name, void *opaque) {
	size_t buf_len;
	uint8_t *buf;

	buf = js_load_file(ctx, &buf_len, module_name);
	if (!buf) {
		JS_ThrowReferenceError(ctx, "could not load module filename '%s'",
							   module_name);
		return NULL;
	}

	JSValue func_val;
	/* compile the module */
	func_val = JS_Eval(ctx, (char *)buf, buf_len, module_name,
					   JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
	js_free(ctx, buf);
	if (JS_IsException(func_val))
		return NULL;
	/* XXX: could propagate the exception */
	js_module_set_import_meta(ctx, func_val, 1, 0);
	/* the module is already referenced, so we must free it */
	JSModuleDef *m = JS_VALUE_GET_PTR(func_val);
	JS_FreeValue(ctx, func_val);

	return m;
}

int setup_quickjs_module(struct Module **mod, const char *filename) {
	JSRuntime *rt = JS_NewRuntime();

	JSContext *ctx = JS_NewContext(rt);
	JS_SetContextOpaque(ctx, NULL);
	js_std_add_helpers(ctx, 0, NULL);

	JS_AddModuleExport(ctx, js_init_module_wifi(ctx, "pak:wifi"), "WiFi");
	JS_AddModuleExport(ctx, js_init_module_pak_runtime(ctx, "pak:runtime"), "Module");
	js_init_module_socket(ctx, "c:socket");
	js_init_module_std(ctx, "qjs:std");
	JS_SetModuleLoaderFunc(rt, NULL, my_module_loader, NULL);
	js_std_init_handlers(rt);

	FILE *file = fopen(filename, "rb");
	if (!file) {
		printf("Failed to open '%s'\n", filename);
		return -1;
	}
	
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *buffer = (char *)malloc(file_size + 1);
	if (!buffer) return -1;
	
	fread(buffer, 1, file_size, file);
	buffer[file_size] = '\0';

	fclose(file);

	JSValue val = JS_Eval(ctx, buffer, file_size, filename, JS_EVAL_TYPE_MODULE);
	if (JS_IsException(val)) {
		const char *str = JS_ToCString(ctx, val);
		printf("JS error: %s\n", str);
		JS_FreeCString(ctx, str);
		return -1;
	}

	JS_FreeValue(ctx, val);

	struct Module *exported_module = JS_GetContextOpaque(ctx);
	if (exported_module != NULL) {
		exported_module->priv->rt = rt;
		exported_module->free = free_module_and_runtime; // override free method so it closes down quickjs instance
		(*mod) = exported_module;
		return 0;
	}

	js_std_free_handlers(rt);
	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);

	printf("JS didn't export module\n");
	return -1;
}
