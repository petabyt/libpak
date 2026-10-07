struct wasm_args_ctx {
	wasm_module_inst_t inst;
	char *p; // native pointer to the next argument cell
};

typedef int (*printf_out_fn)(void *ctx, char c);

const char *wasm_next_str(void *user);
//static uint32_t wasm_next_int(void *user);

int printf_core(printf_out_fn out, void *out_ctx, const char *fmt, void *next_int_ctx, uint32_t (*next_int)(void *next_int_ctx), void *next_str_ctx, const char *(*next_str)(void *next_str_ctx));

int32_t get_libusb_export_apis(NativeSymbol **funcs);
