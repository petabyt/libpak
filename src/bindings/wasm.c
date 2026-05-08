#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wasm_c_api.h>
#include <wasm_export.h>
#include "runtime.h"

static int pak_global_log_wrapper(wasm_exec_env_t exec_env, char *format, char *va_args) {
    wasm_module_inst_t module_inst = get_module_inst(exec_env);

    /* format has been checked by runtime */
    if (!wasm_runtime_validate_native_addr(module_inst, va_args, sizeof(int32_t)))
        return 0;

	printf("%s\n", format);

	return 0;
}

int setup_wasm_module(struct Module **mod, const char *filename) {
	static char global_heap_buf[512 * 1024];
	char error_buf[128];

	wasm_module_t module = NULL;
	wasm_module_inst_t module_inst = NULL;
	wasm_exec_env_t exec_env = NULL;
	uint32_t stack_size = 8092, heap_size = 8092;
	wasm_function_inst_t func = NULL;
	wasm_function_inst_t func2 = NULL;
	char *native_buffer = NULL;
	uint64_t wasm_buffer = 0;

	RuntimeInitArgs init_args;
	memset(&init_args, 0, sizeof(RuntimeInitArgs));

	static NativeSymbol native_symbols[] = {
		{"pak_global_log", pak_global_log_wrapper, "($*)", NULL},
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

	FILE *file = fopen(filename, "rb");
	if (!file) {
		printf("Failed to open '%s'\n", filename);
		return -1;
	}
	
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	uint8_t *buffer = malloc(file_size + 1);
	if (!buffer) return -1;
	
	fread(buffer, 1, file_size, file);
	buffer[file_size] = '\0';

	fclose(file);

	module = wasm_runtime_load(buffer, file_size, error_buf,
							   sizeof(error_buf));
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

	struct Module native_mod = {0};

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
	if (buffer)
		free(buffer);
	wasm_runtime_destroy();
	return 0;
}
