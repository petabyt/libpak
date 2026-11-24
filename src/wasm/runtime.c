#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <wasm_c_api.h>
#include <wasm_export.h>
#include <runtime.h>
#include "main.h"
#include "layout.h"
#include "memory.h"

/* The guest ABI is wasm32; its pointers and function table indices are not
 * host pointers.  Keep the guest module separate from the host module. */
struct WasmModuleContext {
	struct PakModule *host;
	wasm_module_t module;
	wasm_module_inst_t inst;
	wasm_exec_env_t env;
	uint8_t *bytes;
	uint32_t guest;
	uint32_t setup_option;
	struct WasmModuleContext *next;
};

static struct WasmModuleContext *contexts;

static struct WasmModuleContext *from_inst(wasm_module_inst_t inst) {
	for (struct WasmModuleContext *c = contexts; c; c = c->next)
		if (c->inst == inst)
			return c;
	return NULL;
}

static struct WasmModuleContext *from_host(struct PakModule *mod) {
	for (struct WasmModuleContext *c = contexts; c; c = c->next)
		if (c->host == mod)
			return c;
	return NULL;
}

static void *guest_data(wasm_module_inst_t inst, uint32_t addr, uint32_t size) {
	return addr ? wasm_guest_addr(inst, addr, size) : NULL;
}

static const char *guest_string(wasm_module_inst_t inst, uint32_t addr) {
	if (!addr)
		return NULL;
	if (!wasm_runtime_validate_app_str_addr(inst, addr))
		return NULL;
	return wasm_runtime_addr_app_to_native(inst, addr);
}

static struct PakModule *host_module(wasm_exec_env_t env, uint32_t guest) {
	struct WasmModuleContext *c = from_inst(wasm_runtime_get_module_inst(env));
	if (!c || guest != c->guest) {
		wasm_runtime_set_exception(wasm_runtime_get_module_inst(env), "invalid PakModule");
		return NULL;
	}
	return c->host;
}

static int stdout_out(void *user, char c) {
	(void)user;
	return putchar(c) == EOF ? 1 : 0;
}

// wasm wrapper's argument providers: every argument is a 4 byte cell in
// guest linear memory, byte composed so the wrapper stays host endian
// agnostic; each read is bounds checked
uint32_t wasm_next_int(void *user) {
	struct WasmArgsCtx *ctx = (struct WasmArgsCtx *)user;
	const uint8_t *p;

	if (!wasm_runtime_validate_native_addr(ctx->inst, ctx->p, 4)) {
		ctx->inst = NULL;
		return 0;
	}

	p = (uint8_t *)ctx->p;
	ctx->p += 4;
	return wasm_read_u32le(p);
}

static int pak_global_log_wrapper(wasm_exec_env_t exec_env, char *format, char *va_args) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct WasmArgsCtx args = {.inst = inst, .p = va_args};
	int32_t ret;

	// format was validated by the runtime ("$")
	ret = printf_core(stdout_out, NULL, format, &args, wasm_next_int, &args, wasm_next_str);

	// if an argument read failed the instance carries an exception; make
	// sure the failure is not silently treated as success
	if (args.inst == NULL && !wasm_runtime_get_exception(inst))
		wasm_runtime_set_exception(inst, "printf2: invalid argument access");

	return ret;
}

static int pak_debug_log_wrapper(wasm_exec_env_t exec_env, int32_t mod, char *format, char *va_args) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct WasmArgsCtx args = {.inst = inst, .p = va_args};

	printf_core(stdout_out, NULL, format, &args, wasm_next_int, &args, wasm_next_str);

	if (args.inst == NULL && !wasm_runtime_get_exception(inst))
		wasm_runtime_set_exception(inst, "printf2: invalid argument access");

	return 0;
}

static int set_property(wasm_exec_env_t e, uint32_t m, const char *key, const char *value) {
	struct PakModule *h = host_module(e, m);
	return h ? pak_rt_set_session_property(h, key, value) : -1;
}

static int set_property_int(wasm_exec_env_t e, uint32_t m, const char *key, int value) {
	struct PakModule *h = host_module(e, m);
	return h ? pak_rt_set_session_property_int(h, key, value) : -1;
}

static int set_screen(wasm_exec_env_t e, uint32_t m, int screen, int value) {
	struct PakModule *h = host_module(e, m);
	return h ? pak_rt_set_screen_supported(h, screen, value) : -1;
}

static int set_tick(wasm_exec_env_t e, uint32_t m, uint32_t us) {
	struct PakModule *h = host_module(e, m);
	return h ? pak_rt_set_tick_interval(h, us) : -1;
}

static int set_progress(wasm_exec_env_t e, uint32_t m, int job, int percent) {
	struct PakModule *h = host_module(e, m);
	return h ? pak_rt_set_progress_bar(h, job, percent) : -1;
}

static int is_cancelled(wasm_exec_env_t e, uint32_t m, int job) {
	struct PakModule *h = host_module(e, m);
	return h ? pak_rt_is_job_cancelled(h, job) : 1;
}

static uint32_t get_setup(wasm_exec_env_t e, uint32_t m) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(e);
	struct WasmModuleContext *c = from_inst(inst);
	struct PakModule *h = host_module(e, m);
	if (!h)
		return 0;
	const char *s = pak_rt_get_setup_option(h);
	if (c->setup_option)
		wasm_runtime_module_free(inst, c->setup_option);
	c->setup_option = 0;
	if (s) {
		void *p = NULL;
		c->setup_option = wasm_runtime_module_malloc(inst, strlen(s) + 1, &p);
		if (p)
			memcpy(p, s, strlen(s) + 1);
	}
	return c->setup_option;
}

static int set_storage(wasm_exec_env_t e, uint32_t m, const char *name, uint32_t ptr) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(e);
	struct PakModule *h = host_module(e, m);
	const uint8_t *p = guest_data(inst, ptr, 32);
	if (!h || !p)
		return -1;
	struct PakStorageInfo info = {.n_files_root = wasm_read_u32le(p),
								  .sorted_by = wasm_read_u32le(p + 4),
								  .type = wasm_read_u32le(p + 8),
								  .size_bytes = wasm_read_u64le(p + 16),
								  .used_bytes = wasm_read_u64le(p + 24)};
	return pak_rt_set_storage_info(h, name, &info);
}

static int set_widget(wasm_exec_env_t e, uint32_t m, const char *name, uint32_t ptr) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(e);
	struct PakModule *h = host_module(e, m);
	if (!h)
		return -1;
	if (!ptr)
		return pak_rt_set_widget(h, name, NULL);
	const uint8_t *p = guest_data(inst, ptr, 36);
	if (!p)
		return -1;
	struct PakWidget w = {0};
	w.title = guest_string(inst, wasm_read_u32le(p));
	w.group = wasm_read_u32le(p + 4);
	w.disabled = wasm_read_u32le(p + 8);
	w.type = wasm_read_u32le(p + 12);
	const char **list = NULL;
	switch (w.type) {
	case PAK_BUTTON:
		break;
	case PAK_BOOLEAN:
	case PAK_NUMBER:
		w.u.numberv.v = wasm_read_u32le(p + 16);
		break;
	case PAK_SLIDER:
		w.u.sliderv.v = wasm_read_u32le(p + 16);
		w.u.sliderv.min = wasm_read_u32le(p + 20);
		w.u.sliderv.max = wasm_read_u32le(p + 24);
		break;
	case PAK_TEXTBOX:
		w.u.textboxv.value = guest_string(inst, wasm_read_u32le(p + 16));
		break;
	case PAK_COMBOBOX: {
		uint32_t off = wasm_read_u32le(p + 16), n = 0;
		while (off) {
			const uint8_t *item = guest_data(inst, off + n * 4, 4);
			if (!item)
				return -1;
			if (!wasm_read_u32le(item))
				break;
			if (++n > 4096)
				return -1;
		}
		list = calloc(n + 1, sizeof(*list));
		if (!list)
			return -1;
		for (uint32_t i = 0; i < n; i++) {
			const uint8_t *item = guest_data(inst, off + i * 4, 4);
			list[i] = guest_string(inst, wasm_read_u32le(item));
			if (!list[i]) {
				free(list);
				return -1;
			}
		}
		w.u.comboboxv.list = list;
		w.u.comboboxv.index_value = wasm_read_u32le(p + 20);
		break;
	}
	default:
		wasm_runtime_set_exception(inst, "unsupported widget type");
		return -1;
	}
	int rc = pak_rt_set_widget(h, name, &w);
	free(list);
	return rc;
}

static int decode_handle(wasm_module_inst_t inst, uint32_t ptr, struct PakObjectHandle *h) {
	const uint8_t *p = guest_data(inst, ptr, 12);
	if (!p)
		return -1;
	h->index_in_view = wasm_read_u32le(p);
	h->storage_name = guest_string(inst, wasm_read_u32le(p + 4));
	h->path = guest_string(inst, wasm_read_u32le(p + 8));
	return wasm_runtime_get_exception(inst) ? -1 : 0;
}

static int add_metadata(wasm_exec_env_t e, uint32_t m, uint32_t handle, uint32_t metadata) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(e);
	struct PakModule *h = host_module(e, m);
	struct PakObjectHandle file;
	if (!h || decode_handle(inst, handle, &file))
		return -1;
	if (!metadata)
		return pak_rt_add_file_metadata(h, &file, NULL);
	const uint8_t *p = guest_data(inst, metadata, 32);
	if (!p)
		return -1;
	struct PakFileMetadata md = {.filename = guest_string(inst, wasm_read_u32le(p)),
								 .mime_type = guest_string(inst, wasm_read_u32le(p + 4)),
								 .file_size = wasm_read_u64le(p + 8),
								 .image_width = wasm_read_u32le(p + 16),
								 .image_height = wasm_read_u32le(p + 20),
								 .orientation = wasm_read_u32le(p + 24),
								 .created_date = guest_string(inst, wasm_read_u32le(p + 28))};
	if (wasm_runtime_get_exception(inst))
		return -1;
	return pak_rt_add_file_metadata(h, &file, &md);
}

static int add_thumbnail(wasm_exec_env_t e, uint32_t m, uint32_t handle, uint32_t data, uint32_t len) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(e);
	struct PakModule *h = host_module(e, m);
	struct PakObjectHandle file;
	if (!h || decode_handle(inst, handle, &file))
		return -1;
	void *p = data ? guest_data(inst, data, len) : NULL;
	if (data && !p)
		return -1;
	return pak_rt_add_file_thumbnail(h, &file, p, len);
}

static int add_contents(wasm_exec_env_t e, uint32_t m, uint32_t handle, uint32_t data, uint32_t len, uint64_t offset,
						uint64_t total) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(e);
	struct PakModule *h = host_module(e, m);
	struct PakObjectHandle file;
	if (!h || decode_handle(inst, handle, &file))
		return -1;
	void *p = data ? guest_data(inst, data, len) : NULL;
	if (data && !p)
		return -1;
	return pak_rt_add_file_contents(h, &file, p, len, offset, total);
}

static int wasm_usleep(wasm_exec_env_t e, uint32_t usec) {
	(void)e;
	return usleep(usec);
}

static int call_guest(struct WasmModuleContext *c, unsigned offset, uint32_t *args, unsigned argc) {
	const uint8_t *p = guest_data(c->inst, c->guest + offset, 4);
	if (!p || !wasm_read_u32le(p))
		return -1;
	if (!wasm_runtime_call_indirect(c->env, wasm_read_u32le(p), argc, args)) {
		fprintf(stderr, "WASM callback failed: %s\n", wasm_runtime_get_exception(c->inst));
		return -1;
	}
	return (int32_t)args[0];
}

static int guest_init(struct PakModule *m) {
	struct WasmModuleContext *c = from_host(m);
	uint32_t a[] = {c->guest};
	return call_guest(c, WASM_OFFSETOF_PAK_MODULE_INIT, a, 1);
}

static int guest_find(struct PakModule *m, int job) {
	struct WasmModuleContext *c = from_host(m);
	uint32_t a[] = {c->guest, job};
	return call_guest(c, WASM_OFFSETOF_PAK_MODULE_ON_FIND_CONNECTION, a, 2);
}

static int guest_tick(struct PakModule *m, unsigned us) {
	struct WasmModuleContext *c = from_host(m);
	uint32_t a[] = {c->guest, us};
	return call_guest(c, WASM_OFFSETOF_PAK_MODULE_ON_IDLE_TICK, a, 2);
}

static int guest_disconnect(struct PakModule *m) {
	struct WasmModuleContext *c = from_host(m);
	uint32_t a[] = {c->guest};
	return call_guest(c, WASM_OFFSETOF_PAK_MODULE_ON_DISCONNECT, a, 1);
}

static int guest_screen(struct PakModule *m, int old, int new_screen, int job) {
	struct WasmModuleContext *c = from_host(m);
	uint32_t a[] = {c->guest, old, new_screen, job};
	return call_guest(c, WASM_OFFSETOF_PAK_MODULE_ON_SWITCH_SCREEN, a, 4);
}

static uint32_t copy_string(struct WasmModuleContext *c, const char *s) {
	if (!s)
		return 0;
	void *p = NULL;
	size_t n = strlen(s) + 1;
	if (n > UINT32_MAX)
		return 0;
	uint32_t off = wasm_runtime_module_malloc(c->inst, n, &p);
	if (p)
		memcpy(p, s, n);
	return off;
}

static int guest_file(struct PakModule *m, int job, struct PakObjectHandle *file, unsigned offset) {
	struct WasmModuleContext *c = from_host(m);
	if (!file)
		return -1;
	uint32_t storage = copy_string(c, file->storage_name);
	uint32_t path = copy_string(c, file->path);
	void *p = NULL;
	uint32_t handle = wasm_runtime_module_malloc(c->inst, WASM_SIZEOF_PAK_OBJECT_HANDLE, &p);
	int rc = -1;
	if (handle && (!file->storage_name || storage) && (!file->path || path)) {
		wasm_put_u32le((uint8_t *)p + WASM_OFFSETOF_PAK_OBJECT_HANDLE_INDEX_IN_VIEW, file->index_in_view);
		wasm_put_u32le((uint8_t *)p + WASM_OFFSETOF_PAK_OBJECT_HANDLE_STORAGE_NAME, storage);
		wasm_put_u32le((uint8_t *)p + WASM_OFFSETOF_PAK_OBJECT_HANDLE_PATH, path);
		uint32_t a[] = {c->guest, job, handle};
		rc = call_guest(c, offset, a, 3);
	}
	if (handle)
		wasm_runtime_module_free(c->inst, handle);
	if (storage)
		wasm_runtime_module_free(c->inst, storage);
	if (path)
		wasm_runtime_module_free(c->inst, path);
	return rc;
}

static int guest_contents(struct PakModule *m, int j, struct PakObjectHandle *f) {
	return guest_file(m, j, f, WASM_OFFSETOF_PAK_MODULE_ON_REQUEST_FILE_CONTENTS);
}

static int guest_thumbnail(struct PakModule *m, int j, struct PakObjectHandle *f) {
	return guest_file(m, j, f, WASM_OFFSETOF_PAK_MODULE_ON_REQUEST_FILE_THUMBNAIL);
}

static int guest_metadata(struct PakModule *m, int j, struct PakObjectHandle *f) {
	return guest_file(m, j, f, WASM_OFFSETOF_PAK_MODULE_ON_REQUEST_OBJECT_INFO);
}

static int guest_liveview(struct PakModule *m, int j, struct PakObjectHandle *f) {
	return guest_file(m, j, f, WASM_OFFSETOF_PAK_MODULE_ON_REQUEST_LIVEVIEW_FRAME);
}

static int guest_test(struct PakModule *m, int job) {
	struct WasmModuleContext *c = from_host(m);
	uint32_t a[] = {c->guest, job};
	return call_guest(c, WASM_OFFSETOF_PAK_MODULE_ON_RUN_TEST, a, 2);
}

static int guest_command(struct PakModule *m, int job, int argc, const char *const *argv) {
	struct WasmModuleContext *c = from_host(m);
	if (argc < 0 || argc > 4096 || (argc && !argv))
		return -1;
	uint32_t *offsets = calloc((size_t)argc + 1, sizeof(*offsets));
	if (!offsets)
		return -1;
	int rc = -1;
	for (int i = 0; i < argc; i++) {
		offsets[i] = copy_string(c, argv[i]);
		if (!offsets[i])
			goto done;
	}
	void *p = NULL;
	uint32_t table = wasm_runtime_module_malloc(c->inst, ((uint32_t)argc + 1) * 4, &p);
	if (!table)
		goto done;
	for (int i = 0; i <= argc; i++)
		wasm_put_u32le((uint8_t *)p + 4 * i, offsets[i]);
	uint32_t a[] = {c->guest, job, argc, table};
	rc = call_guest(c, WASM_OFFSETOF_PAK_MODULE_ON_CUSTOM_COMMAND, a, 4);
	wasm_runtime_module_free(c->inst, table);
done:
	for (int i = 0; i < argc; i++)
		if (offsets[i])
			wasm_runtime_module_free(c->inst, offsets[i]);
	free(offsets);
	return rc;
}

/* The dummy's widget callback only inspects the name. Serialize the scalar
 * widget variants used by it; reject variants requiring nested allocations. */
static int guest_widget(struct PakModule *m, int job, const char *name, struct PakWidget *w) {
	struct WasmModuleContext *c = from_host(m);
	if (!name ||
		(w && w->type != PAK_BUTTON && w->type != PAK_BOOLEAN && w->type != PAK_NUMBER && w->type != PAK_SLIDER))
		return -1;
	uint32_t n = copy_string(c, name), title = w ? copy_string(c, w->title) : 0, widget = 0;
	void *p = NULL;
	int rc = -1;
	if (!n || (w && w->title && !title))
		goto done;
	if (w) {
		widget = wasm_runtime_module_malloc(c->inst, 36, &p);
		if (!widget)
			goto done;
		memset(p, 0, 36);
		wasm_put_u32le(p, title);
		wasm_put_u32le((uint8_t *)p + 4, w->group);
		wasm_put_u32le((uint8_t *)p + 8, w->disabled);
		wasm_put_u32le((uint8_t *)p + 12, w->type);
		if (w->type == PAK_BOOLEAN || w->type == PAK_NUMBER)
			wasm_put_u32le((uint8_t *)p + 16, w->u.numberv.v);
		if (w->type == PAK_SLIDER) {
			wasm_put_u32le((uint8_t *)p + 16, w->u.sliderv.v);
			wasm_put_u32le((uint8_t *)p + 20, w->u.sliderv.min);
			wasm_put_u32le((uint8_t *)p + 24, w->u.sliderv.max);
		}
	}
	uint32_t a[] = {c->guest, job, n, widget};
	rc = call_guest(c, WASM_OFFSETOF_PAK_MODULE_ON_WIDGET_UPDATED, a, 4);
done:
	if (widget)
		wasm_runtime_module_free(c->inst, widget);
	if (title)
		wasm_runtime_module_free(c->inst, title);
	if (n)
		wasm_runtime_module_free(c->inst, n);
	return rc;
}

static int guest_free(struct PakModule *m) {
	struct WasmModuleContext *c = from_host(m);
	if (!c)
		return -1;
	const uint8_t *p = guest_data(c->inst, c->guest + WASM_OFFSETOF_PAK_MODULE_FREE, 4);
	int rc = 0;
	if (p && wasm_read_u32le(p)) {
		uint32_t a[] = {c->guest};
		rc = call_guest(c, WASM_OFFSETOF_PAK_MODULE_FREE, a, 1);
	}
	wasm_bt_detach(c->inst);
	wasm_wifi_detach(c->inst);
	struct WasmModuleContext **link = &contexts;
	while (*link && *link != c)
		link = &(*link)->next;
	if (*link)
		*link = c->next;
	wasm_runtime_destroy_exec_env(c->env);
	wasm_runtime_deinstantiate(c->inst);
	wasm_runtime_unload(c->module);
	free(c->bytes);
	free(c);
	memset(&m->init, 0, sizeof(*m) - offsetof(struct PakModule, init));
	if (!contexts) {
		libusb_native_unregister();
		wasm_runtime_destroy();
	}
	return rc;
}

int setup_wasm_module(struct PakModule *mod, char *file_contents, unsigned int file_length) {
	static NativeSymbol symbols[] = {
		{"pak_global_log", (void *)(uintptr_t)pak_global_log_wrapper, "($*)", NULL},
		{"pak_debug_log", (void *)(uintptr_t)pak_debug_log_wrapper, "(i$*)", NULL},
		{"pak_rt_set_session_property", (void *)(uintptr_t)set_property, "(i$$)i", NULL},
		{"pak_rt_set_session_property_int", (void *)(uintptr_t)set_property_int, "(i$i)i", NULL},
		{"pak_rt_set_screen_supported", (void *)(uintptr_t)set_screen, "(iii)i", NULL},
		{"pak_rt_set_tick_interval", (void *)(uintptr_t)set_tick, "(ii)i", NULL},
		{"pak_rt_set_progress_bar", (void *)(uintptr_t)set_progress, "(iii)i", NULL},
		{"pak_rt_is_job_cancelled", (void *)(uintptr_t)is_cancelled, "(ii)i", NULL},
		{"pak_rt_get_setup_option", (void *)(uintptr_t)get_setup, "(i)i", NULL},
		{"pak_rt_set_storage_info", (void *)(uintptr_t)set_storage, "(i$i)i", NULL},
		{"pak_rt_set_widget", (void *)(uintptr_t)set_widget, "(i$i)i", NULL},
		{"pak_rt_add_file_metadata", (void *)(uintptr_t)add_metadata, "(iii)i", NULL},
		{"pak_rt_add_file_thumbnail", (void *)(uintptr_t)add_thumbnail, "(iiii)i", NULL},
		{"pak_rt_add_file_contents", (void *)(uintptr_t)add_contents, "(iiiiII)i", NULL},
		{"usleep", (void *)(uintptr_t)wasm_usleep, "(i)i", NULL},
	};
	if (!mod || !file_contents || !file_length || from_host(mod))
		return -1;
	if (!contexts) {
		RuntimeInitArgs init = {0};
		init.mem_alloc_type = Alloc_With_Allocator;
		init.mem_alloc_option.allocator.malloc_func = (void *)(uintptr_t)malloc;
		init.mem_alloc_option.allocator.realloc_func = (void *)(uintptr_t)realloc;
		init.mem_alloc_option.allocator.free_func = (void *)(uintptr_t)free;
		init.n_native_symbols = sizeof(symbols) / sizeof(symbols[0]);
		init.native_module_name = "env";
		init.native_symbols = symbols;
		if (!wasm_runtime_full_init(&init))
			return -1;
		if (!wasm_bt_register()) {
			wasm_runtime_destroy();
			return -1;
		}
		if (!wasm_wifi_register()) {
			wasm_runtime_destroy();
			return -1;
		}
		if (!wasm_socket_register()) {
			wasm_runtime_destroy();
			return -1;
		}
		if (!libusb_native_register()) {
			wasm_runtime_destroy();
			return -1;
		}
	}
	struct WasmModuleContext *c = calloc(1, sizeof(*c));
	char error[256] = {0};
	if (!c)
		goto fail_runtime;
	c->host = mod;
	c->bytes = malloc(file_length);
	if (!c->bytes)
		goto fail;
	memcpy(c->bytes, file_contents, file_length);
	c->module = wasm_runtime_load(c->bytes, file_length, error, sizeof(error));
	if (!c->module)
		goto fail;
	/* Socket compatibility currently grants modules IPv4 network access. */
	static const char *network_pool[] = {"0.0.0.0/0"};
	wasm_runtime_set_wasi_addr_pool(c->module, network_pool, 1);
	c->inst = wasm_runtime_instantiate(c->module, 64 * 1024, 256 * 1024, error, sizeof(error));
	if (!c->inst)
		goto fail;
	if (!wasm_bt_attach(c->inst, mod->bt))
		goto fail;
	if (!wasm_wifi_attach(c->inst, mod->net))
		goto fail;
	c->env = wasm_runtime_create_exec_env(c->inst, 64 * 1024);
	if (!c->env)
		goto fail;
	wasm_function_inst_t fn = wasm_runtime_lookup_function(c->inst, "get_module");
	if (!fn) {
		snprintf(error, sizeof(error), "missing get_module export");
		goto fail;
	}
	void *guest = NULL;
	c->guest = wasm_runtime_module_malloc(c->inst, WASM_SIZEOF_PAK_MODULE, &guest);
	if (!c->guest)
		goto fail;
	memset(guest, 0, WASM_SIZEOF_PAK_MODULE);
	if (mod->net)
		wasm_put_u32le((uint8_t *)guest + WASM_OFFSETOF_PAK_MODULE_NET, 1);
	if (mod->bt)
		wasm_put_u32le((uint8_t *)guest + WASM_OFFSETOF_PAK_MODULE_BT, 1);
	c->next = contexts;
	contexts = c;
	uint32_t args[] = {c->guest};
	if (!wasm_runtime_call_wasm(c->env, fn, 1, args) || (int32_t)args[0] != 0) {
		snprintf(error, sizeof(error), "get_module failed: %s",
				 wasm_runtime_get_exception(c->inst) ? wasm_runtime_get_exception(c->inst) : "nonzero return");
		contexts = c->next;
		goto fail;
	}
	const uint8_t *p = guest_data(c->inst, c->guest, WASM_SIZEOF_PAK_MODULE);
	if (!p) {
		contexts = c->next;
		goto fail;
	}
	if (wasm_read_u32le(p + WASM_OFFSETOF_PAK_MODULE_INIT))
		mod->init = guest_init;
	mod->free = guest_free;
	if (wasm_read_u32le(p + WASM_OFFSETOF_PAK_MODULE_ON_FIND_CONNECTION))
		mod->on_find_connection = guest_find;
	if (wasm_read_u32le(p + WASM_OFFSETOF_PAK_MODULE_ON_IDLE_TICK))
		mod->on_idle_tick = guest_tick;
	if (wasm_read_u32le(p + WASM_OFFSETOF_PAK_MODULE_ON_DISCONNECT))
		mod->on_disconnect = guest_disconnect;
	if (wasm_read_u32le(p + WASM_OFFSETOF_PAK_MODULE_ON_SWITCH_SCREEN))
		mod->on_switch_screen = guest_screen;
	if (wasm_read_u32le(p + WASM_OFFSETOF_PAK_MODULE_ON_REQUEST_FILE_CONTENTS))
		mod->on_request_file_contents = guest_contents;
	if (wasm_read_u32le(p + WASM_OFFSETOF_PAK_MODULE_ON_REQUEST_FILE_THUMBNAIL))
		mod->on_request_file_thumbnail = guest_thumbnail;
	if (wasm_read_u32le(p + WASM_OFFSETOF_PAK_MODULE_ON_REQUEST_OBJECT_INFO))
		mod->on_request_object_info = guest_metadata;
	if (wasm_read_u32le(p + WASM_OFFSETOF_PAK_MODULE_ON_REQUEST_LIVEVIEW_FRAME))
		mod->on_request_liveview_frame = guest_liveview;
	if (wasm_read_u32le(p + WASM_OFFSETOF_PAK_MODULE_ON_WIDGET_UPDATED))
		mod->on_widget_updated = guest_widget;
	if (wasm_read_u32le(p + WASM_OFFSETOF_PAK_MODULE_ON_RUN_TEST))
		mod->on_run_test = guest_test;
	if (wasm_read_u32le(p + WASM_OFFSETOF_PAK_MODULE_ON_CUSTOM_COMMAND))
		mod->on_custom_command = guest_command;
	return 0;
fail:
	fprintf(stderr, "WASM module setup failed: %s\n", error[0] ? error : "out of memory or invalid module");
	if (c->env)
		wasm_runtime_destroy_exec_env(c->env);
	if (c->inst)
		wasm_bt_detach(c->inst);
	if (c->inst)
		wasm_wifi_detach(c->inst);
	if (c->inst)
		wasm_runtime_deinstantiate(c->inst);
	if (c->module)
		wasm_runtime_unload(c->module);
	free(c->bytes);
	free(c);
fail_runtime:
	if (!contexts) {
		libusb_native_unregister();
		wasm_runtime_destroy();
	}
	return -1;
}
