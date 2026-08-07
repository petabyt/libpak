#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <quickjs.h>
#include <quickjs-libc.h>
#pragma GCC diagnostic pop
#include <runtime.h>
#include "../main.h"
#include "buffer_js.h"
#include "http_js.h"

JSModuleDef *js_init_module_socket(JSContext *ctx, const char *module_name);
JSModuleDef *js_init_module_wifi(JSContext *ctx, const char *module_name);

JSValue pak_js_create_adapter(JSContext *ctx, JSValue wifi, struct PakWiFiAdapter *adapter);
JSValue pak_js_create_wifi_context(JSContext *ctx, struct PakNet *wifi_ctx);

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

static JSValue test_module(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[]) {
	JSValue obj = argv[0];
	struct PakModule *mod = JS_GetOpaque(obj, module_class_id);

	if (pak_rt_test_module(mod)) {
		return JS_UNDEFINED;
	}

	return JS_UNDEFINED;
}

static JSValue export_module(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[]) {
	if (JS_GetContextOpaque(ctx) != NULL) {
		JS_SetContextOpaque(ctx, NULL);
	} else {
		printf("PakModule already exported in this context\n");
	}
	return JS_UNDEFINED;
}

static JSValue global_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[]) {
	pak_global_log("%s", JS_ToCString(ctx, argv[0]));
	return JS_UNDEFINED;
}
static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("test", 1, test_module),
	JS_CFUNC_DEF("export", 1, export_module),
	JS_CFUNC_DEF("globalLog", 1, global_log),

#define PAK_CONSTANT(x) JS_PROP_INT32_DEF(#x, x, JS_PROP_CONFIGURABLE)
	PAK_CONSTANT(PAK_SCREEN_DASHBOARD),
	PAK_CONSTANT(PAK_SCREEN_FILE_GALLERY),
	PAK_CONSTANT(PAK_SCREEN_FILE_VIEWER),
	PAK_CONSTANT(PAK_SCREEN_LIVEVIEW),
	PAK_CONSTANT(PAK_NEWEST_FIRST),
	PAK_CONSTANT(PAK_OLDEST_FIRST),
#undef PAK_CONSTANT
#define PAK_CONSTANT(x) JS_PROP_STRING_DEF(#x, x, JS_PROP_CONFIGURABLE)
	PAK_CONSTANT(PAK_PROP_NAME),
	PAK_CONSTANT(PAK_PROP_FW_VER),
#undef PAK_CONSTANT

};

static JSValue debug_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[]) {
	struct PakModule *mod = JS_GetOpaque(this_val, module_class_id);
	pak_debug_log(mod, "%s", JS_ToCString(ctx, argv[0]));
	return JS_UNDEFINED;
}

enum Operations {
	M_SET_SCREEN_SUPPORTED,
	M_ENTER_SCREEN,
	M_ENTER_CUSTOM_SCREEN,
	M_SET_PROGRESS_BAR,
	M_SET_CURRENT_DOWNLOAD_SPEED,
	M_SET_DEVICE_NAME,
	M_SET_STORAGE_INFO,
	M_ADD_FILE_METADATA,
	M_ADD_FILE_THUMBNAIL,
	M_ADD_FILE_CONTENTS,
	M_ADD_WIDGET,
	M_FATAL_ERROR,
	M_SET_PROP
};

struct PakFileHandleWrapper {
	JSValue x;
	JSValue y;
	struct PakFileHandle handle;
};

static struct PakFileHandleWrapper to_file_handle(JSContext *ctx, JSValue obj) {
	JSValue x = JS_GetPropertyStr(ctx, obj, "storageName");
	JSValue y = JS_GetPropertyStr(ctx, obj, "index");
	int index; JS_ToInt32(ctx, &index, y);
	struct PakFileHandleWrapper handle = {
		x, y, {
			.index_in_view = index,
			.storage_name = JS_ToCString(ctx, x),
		}
	};
	return handle;
}

static JSValue generic_operation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[], int magic) {
	struct PakModule *mod = (struct PakModule *)JS_GetOpaque(this_val, module_class_id);
	int rc = 0;
	switch (magic) {
		case M_SET_SCREEN_SUPPORTED: {
			int screen;
			JS_ToInt32(ctx, &screen, argv[0]);
			int v = JS_ToBool(ctx, argv[1]);
			rc = pak_rt_set_screen_supported(mod, screen, v);
		} break;
		case M_SET_PROGRESS_BAR: {
			int percent, job;
			JS_ToInt32(ctx, &job, argv[0]);
			JS_ToInt32(ctx, &percent, argv[1]);
			rc = pak_rt_set_progress_bar(mod, job, percent);
		} break;
		case M_FATAL_ERROR: {
			const char *reason = JS_ToCString(ctx, argv[0]);
			pak_rt_fatal_error(mod, reason);
		} break;
		case M_ADD_FILE_CONTENTS: {
			uint32_t offset, full_length;
			JS_ToUint32(ctx, &offset, argv[2]);
			JS_ToUint32(ctx, &full_length, argv[3]);
			size_t len = 0;
			void *buf = JS_IsNull(argv[1]) ? NULL : JS_GetArrayBuffer(ctx, &len, argv[1]);
			struct PakFileHandleWrapper handle = to_file_handle(ctx, argv[0]);
			rc = pak_rt_add_file_contents(mod, &handle.handle, buf, len, offset, full_length);
			JS_FreeValue(ctx, handle.x);
			JS_FreeValue(ctx, handle.y);
		} break;
		case M_ADD_FILE_THUMBNAIL: {
			size_t len;
			void *buf = JS_GetArrayBuffer(ctx, &len, argv[1]);
			struct PakFileHandleWrapper handle = to_file_handle(ctx, argv[0]);
			rc = pak_rt_add_file_thumbnail(mod, &handle.handle, buf, len);
			JS_FreeValue(ctx, handle.x);
			JS_FreeValue(ctx, handle.y);
		} break;
		case M_ADD_FILE_METADATA: {
			struct PakFileHandleWrapper handle = to_file_handle(ctx, argv[0]);
			JSValue a = JS_GetPropertyStr(ctx, argv[1], "filename");
			JSValue b = JS_GetPropertyStr(ctx, argv[1], "fileSize");
			JSValue c = JS_GetPropertyStr(ctx, argv[1], "mimeType");
			struct PakFileMetadata metadata = {
				.filename = JS_ToCString(ctx, a),
				.mime_type = JS_ToCString(ctx, c),
			};
			JS_ToInt64(ctx, (int64_t *)&metadata.file_size, b);
			rc = pak_rt_add_file_metadata(mod, &handle.handle, &metadata);
			JS_FreeValue(ctx, a); JS_FreeValue(ctx, b); JS_FreeValue(ctx, c);
			JS_FreeValue(ctx, handle.x);
			JS_FreeValue(ctx, handle.y);
		} break;
		case M_SET_PROP: {
			const char *name = JS_ToCString(ctx, argv[0]);
			if (!JS_IsString(argv[0])) return JS_NewError(ctx);
			if (JS_IsString(argv[1])) {
				rc = pak_rt_set_session_property(mod, name, JS_ToCString(ctx, argv[1]));
			} else {
				int val;
				JS_ToInt32(ctx, &val, argv[1]);
				rc = pak_rt_set_session_property_int(mod, name, val);
			}
		} break;
		case M_SET_STORAGE_INFO: {
			if (!JS_IsString(argv[0])) return JS_NewError(ctx);
			unsigned int n_items;
			int sortedby;
			JS_ToUint32(ctx, &n_items, argv[1]);
			JS_ToInt32(ctx, &sortedby, argv[2]);
			rc = pak_rt_set_storage_info(mod, JS_ToCString(ctx, argv[0]), n_items, sortedby);
		} break;
		case M_ADD_WIDGET: {
			if (!JS_IsObject(argv[0])) return JS_NewError(ctx);
			JSValue a = JS_GetPropertyStr(ctx, argv[0], "name");
			JSValue b = JS_GetPropertyStr(ctx, argv[0], "title");
			JSValue c = JS_GetPropertyStr(ctx, argv[0], "type");
			struct PakWidget widget = {
				.name = JS_ToCString(ctx, a),
				.title = JS_ToCString(ctx, b),
			};
			const char *type = JS_ToCString(ctx, c);
			JSValue d = JS_GetPropertyStr(ctx, argv[0], "value");
			if (!strcmp(type, "button")) {
				widget.type = PAK_BUTTON;
			} else if (!strcmp(type, "bool")) {
				widget.type = PAK_BOOLEAN;
				widget.u.boolv.v = JS_ToBool(ctx, d);
			}
			rc = pak_rt_set_widget(mod, &widget);
			JS_FreeValue(ctx, a);
			JS_FreeValue(ctx, b);
			JS_FreeValue(ctx, c);
			JS_FreeValue(ctx, d);
		} break;
	}
	return JS_NewInt32(ctx, rc);
}

static const JSCFunctionListEntry module_methods[] = {
	JS_CFUNC_DEF("debugLog", 1, debug_log),
	JS_CFUNC_MAGIC_DEF("setScreenSupported", 2, generic_operation, M_SET_SCREEN_SUPPORTED),
	JS_CFUNC_MAGIC_DEF("setProgressBar", 2, generic_operation, M_SET_PROGRESS_BAR),
	JS_CFUNC_MAGIC_DEF("setStorageInfo", 3, generic_operation, M_SET_STORAGE_INFO),
	JS_CFUNC_MAGIC_DEF("addFileMetadata", 2, generic_operation, M_ADD_FILE_METADATA),
	JS_CFUNC_MAGIC_DEF("addFileThumbnail", 2, generic_operation, M_ADD_FILE_THUMBNAIL),
	JS_CFUNC_MAGIC_DEF("addFileContents", 4, generic_operation, M_ADD_FILE_CONTENTS),
	JS_CFUNC_MAGIC_DEF("fatalError", 1, generic_operation, M_FATAL_ERROR),
	JS_CFUNC_MAGIC_DEF("setProperty", 2, generic_operation, M_SET_PROP),
	JS_CFUNC_MAGIC_DEF("addWidget", 1, generic_operation, M_ADD_WIDGET),
};

static int on_init(struct PakModule *mod) { return 0; }

static void js_print_value_write(void *opaque, const char *buf, size_t len) {
	uint8_t *out = opaque;
	size_t l = strlen(opaque);
	memcpy(out + l, buf, len);
	out[l + len] = '\0';
}

static void dump_exception(JSContext *ctx) {
	struct PakModule *mod = JS_GetContextOpaque(ctx);
	JSValue val = JS_GetException(ctx);
	char errorbuf[512] = {0};
	JS_PrintValue(ctx, js_print_value_write, errorbuf, val, NULL);
	if (mod != NULL) pak_debug_log(mod, "<error>Exception: %s", errorbuf);
    JS_FreeValue(ctx, val);
}

static int call_module_method(struct JSContext *ctx, JSValue obj, const char *name, int argc, JSValue *argv) {
	int rc = 0;
	JSValue fun = JS_GetPropertyStr(ctx, obj, name);
	if (!JS_IsFunction(ctx, fun)) {
		return PAK_ERR_UNIMPLEMENTED;
	}
	JSValue rv = JS_Call(ctx, fun, obj, argc, argv);
	if (JS_IsException(rv)) {
		JS_FreeValue(ctx, rv);
		dump_exception(ctx);
		rc = -1;
	}
	for (int i = 0; i < argc; i++) {
		JS_FreeValue(ctx, argv[i]);
	}
	JS_FreeValue(ctx, fun);
	return rc;
}

static int on_free(struct PakModule *mod) {
	JS_UpdateStackTop(mod->priv->rt);
	JSValue args[] = {JS_UNDEFINED};
	return call_module_method(mod->priv->ctx, mod->priv->object, "onFree", 0, args);
}

static int on_try_connect_wifi(struct PakModule *mod, struct PakWiFiAdapter *handle, struct PakSavedConnection *saved, int job) {
	JS_UpdateStackTop(mod->priv->rt);
	JSValue wifi = JS_GetPropertyStr(mod->priv->ctx, mod->priv->object, "wifi");
	JSValue adapter = pak_js_create_adapter(mod->priv->ctx, wifi, handle);
	JS_FreeValue(mod->priv->ctx, wifi);

	JSValue args[] = {
		adapter,
		JS_UNDEFINED,
		JS_NewInt32(mod->priv->ctx, job),
	};
	int rc = call_module_method(mod->priv->ctx, mod->priv->object, "onTryConnectWiFi", 3, args);
	return rc;
}

static int on_find_connection(struct PakModule *mod, int job) {
	JS_UpdateStackTop(mod->priv->rt);
	JSValue args[] = {
		JS_NewInt32(mod->priv->ctx, job),
	};
	return call_module_method(mod->priv->ctx, mod->priv->object, "onFindConnection", 0, args);
}

static int on_run_test(struct PakModule *mod, int job) {
	JS_UpdateStackTop(mod->priv->rt);
	JSValue args[] = {
		JS_NewInt32(mod->priv->ctx, job),
	};
	return call_module_method(mod->priv->ctx, mod->priv->object, "onRunTest", 1, args);
}

static int on_disconnect(struct PakModule *mod) {
	JS_UpdateStackTop(mod->priv->rt);
	return call_module_method(mod->priv->ctx, mod->priv->object, "onDisconnect", 0, NULL);
}

static int on_switch_screen(struct PakModule *mod, int old_screen, int new_screen, int job) {
	JS_UpdateStackTop(mod->priv->rt);
	JSValue args[] = {
		JS_NewInt32(mod->priv->ctx, old_screen),
		JS_NewInt32(mod->priv->ctx, new_screen),
		JS_NewInt32(mod->priv->ctx, job),
	};
	return call_module_method(mod->priv->ctx, mod->priv->object, "onSwitchScreen", 3, args);
}

static inline JSValue JS_NewStringOrNull(JSContext *ctx, const char *str) {
	if (str == NULL) return JS_NULL;
    return JS_NewString(ctx, str);
}

static JSValue create_handle(struct PakModule *mod, struct PakFileHandle *file) {
	JSValue handle = JS_NewObject(mod->priv->ctx);
	JS_SetPropertyStr(mod->priv->ctx, handle, "storageName", JS_NewStringOrNull(mod->priv->ctx, file->storage_name));
	JS_SetPropertyStr(mod->priv->ctx, handle, "index", JS_NewInt32(mod->priv->ctx, file->index_in_view));
	return handle;
}

static int on_request_file_contents(struct PakModule *mod, int job, struct PakFileHandle *file) {
	JS_UpdateStackTop(mod->priv->rt);
	JSValue handle = create_handle(mod, file);
	JSValue args[] = {
		JS_NewInt32(mod->priv->ctx, job),
		handle,
	};
	return call_module_method(mod->priv->ctx, mod->priv->object, "onRequestFileContents", 2, args);
}

static int on_request_thumbnail(struct PakModule *mod, int job, struct PakFileHandle *file) {
	JS_UpdateStackTop(mod->priv->rt);
	JSValue handle = create_handle(mod, file);
	JSValue args[] = {
		JS_NewInt32(mod->priv->ctx, job),
		handle,
	};
	return call_module_method(mod->priv->ctx, mod->priv->object, "onRequestFileThumbnail", 2, args);
}

static int on_request_file_metadata(struct PakModule *mod, int job, struct PakFileHandle *file) {
	JS_UpdateStackTop(mod->priv->rt);
	JSValue handle = create_handle(mod, file);
	JSValue args[] = {
		JS_NewInt32(mod->priv->ctx, job),
		handle,
	};
	int rc = call_module_method(mod->priv->ctx, mod->priv->object, "onRequestFileMetadata", 2, args);
}

static JSValue js_module_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst argv[]) {
	JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	JSValue obj = JS_NewObjectProtoClass(ctx, proto, module_class_id);
	JS_FreeValue(ctx, proto);

	struct PakModule *mod = JS_GetContextOpaque(ctx);
	mod->init = on_init;
	mod->free = on_free;
	mod->on_run_test = on_run_test;
	mod->on_try_connect_wifi = on_try_connect_wifi;
	mod->on_find_connection = on_find_connection;
	mod->on_disconnect = on_disconnect;
	mod->on_switch_screen = on_switch_screen;
	mod->on_request_file_contents = on_request_file_contents;
	mod->on_request_file_thumbnail = on_request_thumbnail;
	mod->on_request_file_metadata = on_request_file_metadata;

	mod->priv = malloc(sizeof(struct ModulePriv));
	mod->priv->object = JS_DupValue(ctx, obj);
	mod->priv->ctx = ctx;

	JS_SetPropertyStr(ctx, mod->priv->object, "wifi", pak_js_create_wifi_context(ctx, mod->net));

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

int free_module_and_runtime(struct PakModule *mod) {
	JS_FreeValue(mod->priv->ctx, mod->priv->object);
	js_std_free_handlers(mod->priv->rt);
	JS_RunGC(mod->priv->rt);

	JS_FreeContext(mod->priv->ctx);

	JSMemoryUsage s = {0};
	JS_ComputeMemoryUsage(mod->priv->rt, &s);
	char buffer[4096] = {0};
	FILE *fp = fmemopen(buffer, sizeof(buffer), "w+");
	JS_DumpMemoryUsage(fp, &s, mod->priv->rt);
	pak_global_log(buffer);

	JS_FreeRuntime(mod->priv->rt);
	return 0;
}

// Copied from quickjs source
static JSModuleDef *my_module_loader(JSContext *ctx, const char *module_name, void *opaque) {
	size_t buf_len;
	uint8_t *buf;
	int is_allocated = 0;

	if (!strcmp(module_name, "pak:buffer")) {
		buf = buffer_js;
		buf_len = buffer_js_len;
	} else if (!strcmp(module_name, "pak:http")) {
		buf = http_js;
		buf_len = http_js_len;
	} else {
		buf = js_load_file(ctx, &buf_len, module_name);
		if (!buf) {
			JS_ThrowReferenceError(ctx, "could not load module filename '%s'", module_name);
			return NULL;
		}
		is_allocated = 1;
	}

	// buf is required to be null terminated, assume file has endline
	buf[buf_len - 1] = '\0'; buf_len--;

	JSValue func_val;
	/* compile the module */
	func_val = JS_Eval(ctx, (char *)buf, buf_len, module_name,
					   JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
	if (is_allocated) js_free(ctx, buf);
	if (JS_IsException(func_val))
		return NULL;
	/* XXX: could propagate the exception */
	js_module_set_import_meta(ctx, func_val, 1, 0);
	/* the module is already referenced, so we must free it */
	JSModuleDef *m = JS_VALUE_GET_PTR(func_val);
	JS_FreeValue(ctx, func_val);

	return m;
}

static JSValue js_console_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	struct PakModule *mod = JS_GetContextOpaque(ctx);
	if (mod == NULL) return JS_UNDEFINED;
    if (JS_IsString(argv[0])) {
		const char *str = JS_ToCString(ctx, argv[0]);
		pak_global_log("%s", str);
		JS_FreeCString(ctx, str);
	} else {
		JSValue val = JS_JSONStringify(ctx, argv[0], JS_UNDEFINED, JS_UNDEFINED);
		const char *str = JS_ToCString(ctx, argv[0]);
		pak_global_log("%s", str);
		JS_FreeCString(ctx, str);
		JS_FreeValue(ctx, val);
	}
    return JS_UNDEFINED;
}

int setup_quickjs_module(struct PakModule *mod, char *file_contents, unsigned int length) {
	JSRuntime *rt = JS_NewRuntime();

	if (file_contents[length - 1] == '\0') length--;

	JSContext *ctx = JS_NewContext(rt);
	JS_SetContextOpaque(ctx, mod);
	js_std_add_helpers(ctx, 0, NULL);

	JS_AddModuleExport(ctx, js_init_module_wifi(ctx, "pak:wifi"), "WiFi");
	JS_AddModuleExport(ctx, js_init_module_pak_runtime(ctx, "pak:runtime"), "Module");
	js_init_module_socket(ctx, "c:socket");
	js_init_module_std(ctx, "qjs:std");
	JS_SetModuleLoaderFunc(rt, NULL, my_module_loader, NULL);
	js_std_init_handlers(rt);

	JSValue global_obj = JS_GetGlobalObject(ctx);
	JSValue console = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, console, "log", JS_NewCFunction(ctx, js_console_log, "log", 1));
	JS_SetPropertyStr(ctx, global_obj, "console", console);
	JS_FreeValue(ctx, global_obj);

	// partially copied from qjs.c
	JSValue val = JS_Eval(ctx, file_contents, length, "main.js", JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
	if (JS_IsException(val)) {
		dump_exception(ctx);
		return -1;
	}
	val = JS_EvalFunction(ctx, val);
	val = js_std_await(ctx, val);
	if (JS_IsException(val)) {
		dump_exception(ctx);
		return -1;
	}
	JS_FreeValue(ctx, val);

	int rc = 0;
	if (JS_GetContextOpaque(ctx) != NULL) {
		pak_debug_log(mod, "JS script didn't export a module");
		rc = -1;
	} else {
		mod->priv->rt = rt;
		mod->free = free_module_and_runtime;
		JS_SetContextOpaque(ctx, mod);
		return 0;
	}

	if (mod->priv != NULL) JS_FreeValue(ctx, mod->priv->object);
	js_std_free_handlers(rt);
	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);
	return rc;
}
