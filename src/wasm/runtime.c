#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wasm_c_api.h>
#include <wasm_export.h>
#include "runtime.h"

typedef int (*printf2_out_fn)(void *ctx, char c);

struct wasm_args_ctx {
	wasm_module_inst_t inst;
	uint8_t *p; // native pointer to the next argument cell
};

static int emit_decimal(printf2_out_fn out, void *out_ctx, uint32_t value, int *count) {
	char buf[10];
	uint32_t i = 0;

	// 32 bit decimal is at most 10 digits; 0 emits "0"
	do {
		buf[i++] = (char)('0' + value % 10);
		value /= 10;
	} while (value);

	while (i > 0) {
		if (out(out_ctx, buf[--i]))
			return -1;
		(*count)++;
	}

	return 0;
}

static int emit_hex(printf2_out_fn out, void *out_ctx, uint32_t value, int *count) {
	static const char digits[] = "0123456789abcdef";
	char buf[8];
	uint32_t i = 0;

	do {
		buf[i++] = digits[value & 0xf];
		value >>= 4;
	} while (value);

	while (i > 0) {
		if (out(out_ctx, buf[--i]))
			return -1;
		(*count)++;
	}

	return 0;
}

int printf2_core(printf2_out_fn out, void *out_ctx, const char *fmt, void *next_int_ctx,
                 uint32_t (*next_int)(void *next_int_ctx), void *next_str_ctx,
                 const char *(*next_str)(void *next_str_ctx)) {
	int count = 0;

	if (!fmt)
		return -1;

	while (*fmt) {
		char c = *fmt++;

		if (c != '%') {
			if (out(out_ctx, c))
				return count;
			count++;
			continue;
		}

		// lone '%' at the end of the format string
		if (!*fmt) {
			if (out(out_ctx, '%'))
				return count;
			count++;
			break;
		}

		switch (*fmt++) {
			case 'd':
			{
				// INT_MIN is negated in the unsigned domain
				uint32_t u = next_int_ctx ? next_int(next_int_ctx) : 0;
				uint32_t mag = (u & 0x80000000u) ? (uint32_t) - (int32_t)u : u;

				if (u & 0x80000000u) {
					if (out(out_ctx, '-'))
						return count;
					count++;
				}

				if (emit_decimal(out, out_ctx, mag, &count))
					return count;
				break;
			}
			case 'x':
				if (emit_hex(out, out_ctx, next_int_ctx ? next_int(next_int_ctx) : 0, &count))
					return count;
				break;
			case 's':
			{
				const char *s = next_str_ctx ? next_str(next_str_ctx) : NULL;

				if (!s)
					s = "(null)";
				while (*s) {
					if (out(out_ctx, *s++))
						return count;
					count++;
				}
				break;
			}
			case '%':
				if (out(out_ctx, '%'))
					return count;
				count++;
				break;
			default:
				// unknown specifier: emit verbatim, consume nothing
				if (out(out_ctx, '%'))
					return count;
				if (out(out_ctx, fmt[-1]))
					return count;
				count += 2;
				break;
		}
	}

	return count;
}

// wasm wrapper's argument providers: every argument is a 4 byte cell in
// guest linear memory, byte composed so the wrapper stays host endian
// agnostic; each read is bounds checked
static uint32_t wasm_next_int(void *user) {
	struct wasm_args_ctx *ctx = (struct wasm_args_ctx *)user;
	const uint8_t *p;

	if (!wasm_runtime_validate_native_addr(ctx->inst, ctx->p, 4)) {
		ctx->inst = NULL;
		return 0;
	}

	p = ctx->p;
	ctx->p += 4;
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static const char *wasm_next_str(void *user) {
	struct wasm_args_ctx *ctx = (struct wasm_args_ctx *)user;
	const uint8_t *p;
	uint32_t app_addr;

	if (!wasm_runtime_validate_native_addr(ctx->inst, ctx->p, 4)) {
		ctx->inst = NULL;
		return NULL;
	}

	p = ctx->p;
	app_addr = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
	ctx->p += 4;

	if (app_addr == 0)
		return NULL;

	if (!wasm_runtime_validate_app_str_addr(ctx->inst, app_addr)) {
		ctx->inst = NULL;
		return NULL;
	}

	return (const char *)wasm_runtime_addr_app_to_native(ctx->inst, app_addr);
}

static int stdout_out(void *user, char c) {
	(void)user;
	return putchar(c) == EOF ? 1 : 0;
}

static int pak_global_log_wrapper(wasm_exec_env_t exec_env, char *format, char *va_args) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct wasm_args_ctx args = { .inst = inst, .p = va_args };
	int32_t ret;

	// format was validated by the runtime ("$")
	ret = printf2_core(stdout_out, NULL, format, &args, wasm_next_int, &args, wasm_next_str);

	// if an argument read failed the instance carries an exception; make
	// sure the failure is not silently treated as success
	if (args.inst == NULL && !wasm_runtime_get_exception(inst))
		wasm_runtime_set_exception(inst, "printf2: invalid argument access");

	return ret;
}

static int pak_debug_log_wrapper(wasm_exec_env_t exec_env, int32_t mod, char *format, char *va_args) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
	struct wasm_args_ctx args = { .inst = inst, .p = va_args };
	int32_t ret;

	ret = printf2_core(stdout_out, NULL, format, &args, wasm_next_int, &args, wasm_next_str);

	if (args.inst == NULL && !wasm_runtime_get_exception(inst))
		wasm_runtime_set_exception(inst, "printf2: invalid argument access");

	return 0;
}

int setup_wasm_module(struct PakModule *mod, char *file_contents, unsigned int file_length) {
	static char global_heap_buf[512 * 1024];
	char error_buf[128];

	wasm_module_t module = NULL;
	wasm_module_inst_t module_inst = NULL;
	wasm_exec_env_t exec_env = NULL;
	uint32_t stack_size = 8092, heap_size = 8092;
	wasm_function_inst_t func = NULL;
	uint64_t wasm_buffer = 0;

	RuntimeInitArgs init_args;
	memset(&init_args, 0, sizeof(RuntimeInitArgs));

	static NativeSymbol native_symbols[] = {
		// Ignore pedantic warning - requires (void *) function pointer
		{"pak_global_log", (void *)pak_global_log_wrapper, "($*)", NULL},
		{"pak_debug_log", (void *)pak_debug_log_wrapper, "(i$*)", NULL},
	};

	init_args.mem_alloc_type = Alloc_With_Pool;
	init_args.mem_alloc_option.pool.heap_buf = global_heap_buf;
	init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap_buf);

	// Native symbols need below registration phase
	init_args.n_native_symbols = sizeof(native_symbols) / sizeof(NativeSymbol);
	init_args.native_module_name = "env";
	init_args.native_symbols = native_symbols;

	if (!wasm_runtime_full_init(&init_args)) {
		printf("Init runtime environment failed.\n");
		return -1;
	}
	wasm_runtime_set_log_level(WASM_LOG_LEVEL_VERBOSE);

	// Unregister pthread (TODO: Permission system)

	module = wasm_runtime_load((uint8_t *)file_contents, file_length, error_buf, sizeof(error_buf));
	if (!module) {
		printf("Load wasm module failed. error: %s\n", error_buf);
		goto fail;
	}

	module_inst = wasm_runtime_instantiate(module, stack_size, heap_size,
										   error_buf, sizeof(error_buf));

	if (!module_inst) {
		printf("Instantiate wasm module failed. error: %s\n", error_buf);
		goto fail;
	}

	exec_env = wasm_runtime_create_exec_env(module_inst, stack_size);
	if (!exec_env) {
		printf("Create wasm execution environment failed.\n");
		goto fail;
	}

	if (!(func = wasm_runtime_lookup_function(module_inst, "get_module"))) {
		printf("The get_module wasm function is not found.\n");
		goto fail;
	}

	struct PakModule native_mod = {0};

	uint32_t args[] = {
		wasm_runtime_module_malloc(module_inst, 1000, (void **)&native_mod)
	};
	if (!wasm_runtime_call_wasm(exec_env, func, 1, args)) {
		printf("call wasm function get_module failed. %s\n", wasm_runtime_get_exception(module_inst));
		goto fail;
	}

	printf("return value of call: %d\n", args[0]);

fail:
	if (exec_env)
		wasm_runtime_destroy_exec_env(exec_env);
	if (module_inst) {
		if (wasm_buffer)
			wasm_runtime_module_free(module_inst, (uint64_t)wasm_buffer);
		wasm_runtime_deinstantiate(module_inst);
	}
	if (module)
		wasm_runtime_unload(module);
	wasm_runtime_destroy();
	return 0;
}
