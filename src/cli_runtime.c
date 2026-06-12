#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include "wifi.h"
#include "bluetooth.h"
#include "runtime.h"

struct RuntimePriv {
	int current_job;
};

__attribute__((weak)) int setup_quickjs_module(struct Module *mod, char *file_contents, unsigned int length) {
	pak_debug_log(mod, "quickjs support not compiled in");
	return -1;
}
//__attribute__((weak)) int setup_wasm_module(struct Module **mod, const char *filename) {
//	return -1;
//}

struct Module *pak_create_mod(void) {
	struct Module *mod = calloc(1, sizeof(struct Module));
	mod->rt = malloc(sizeof(struct RuntimePriv));
	mod->rt->current_job = 1;
	return mod;
}

struct Module *pak_rt_mod_from_native(int (*get)(struct Module *mod)) {
	struct Module *mod = pak_create_mod();
	get(mod);
	return mod;
}

static int new_job(struct RuntimePriv *r) {
	return r->current_job++;
}

void pak_debug_log(struct Module *mod, const char *fmt, ...) {
	printf("pak_debug_log: ");
	fflush(stdout);
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	putchar('\n');
	fflush(stdout);
	abort();
}

int pak_rt_test_module(struct Module *mod) {
	// runtime was not inited by pak_create_mod
	mod->rt = malloc(sizeof(struct RuntimePriv));
	mod->rt->current_job = 1;

	struct RuntimePriv *r = mod->rt;
	if (mod->init) mod->init(mod);
	if (mod->on_find_connection) {
		if (mod->on_find_connection(mod, new_job(r))) {
			printf("Failed to find connection\n");
			return -1;
		}
	}
	if (mod->on_run_test) {
		if (mod->on_run_test(mod, SCREEN_CONSOLE, new_job(r))) {
			printf("on_run_test\n");
			return -1;
		}
	}
	if (mod->on_disconnect) {
		if (mod->on_disconnect(mod)) {
			printf("on_disconnect\n");
			return -1;
		}
	}
	return 0;
}
