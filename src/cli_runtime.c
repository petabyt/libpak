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

int pak_rt_save_session_signature(struct PakModule *mod, struct PakSavedConnection *info) {
	return -1;
}
int pak_rt_set_session_property(struct PakModule *mod, const char *key, const char *value) {
	return -1;
}
int pak_rt_set_session_property_int(struct PakModule *mod, const char *key, int value) {
	return -1;
}
int pak_rt_add_file_thumbnail(struct PakModule *mod, struct PakFileHandle *file, void *image_data, unsigned int length) {
	return -1;
}
int pak_rt_add_file_contents(struct PakModule *mod, struct PakFileHandle *file, void *image_data, unsigned int length, int is_partial) {
	return -1;
}
int pak_rt_set_storage_info(struct PakModule *mod, const char *storage_name, unsigned int n_items, enum SortedBy sorted_by) {
	return -1;
}
int pak_rt_set_progress_bar(struct PakModule *mod, int job, int percent) {
	return -1;
}
int pak_rt_is_job_cancelled(struct PakModule *mod, int job) {
	return -1;
}
int pak_rt_set_screen_supported(struct PakModule *mod, int screen, int v) {
	return -1;
}
const char *pak_rt_get_client_name(void) {
	return (const char *)"client";
}
int pak_rt_add_file_metadata(struct PakModule *mod, struct PakFileHandle *file, const struct PakFileMetadata *metadata) {
	return -1;
}
void pak_rt_release_metadata(struct PakModule *mod, struct PakFileMetadata *md) {}
struct PakFileMetadata *pak_rt_get_metadata(struct PakModule *mod, struct PakFileHandle *file) {
	return NULL;
}
void pak_rt_fatal_error(struct PakModule *mod, const char *fmt, ...) {}
const char *pak_rt_get_setup_option(struct PakModule *mod) {
	return NULL;
}
int pak_rt_set_tick_interval(struct PakModule *mod, unsigned int us) {
	return -1;
}

__attribute__((weak)) int setup_quickjs_module(struct PakModule *mod, char *file_contents, unsigned int length) {
	pak_debug_log(mod, "quickjs support not compiled in");
	return -1;
}
//__attribute__((weak)) int setup_wasm_module(struct PakModule **mod, const char *filename) {
//	return -1;
//}

struct PakModule *pak_create_mod(void) {
	struct PakModule *mod = calloc(1, sizeof(struct PakModule));
	mod->rt = malloc(sizeof(struct RuntimePriv));
	mod->rt->current_job = 1;
	return mod;
}

struct PakModule *pak_rt_mod_from_native(int (*get)(struct PakModule *mod)) {
	struct PakModule *mod = pak_create_mod();
	get(mod);
	return mod;
}

static int new_job(struct RuntimePriv *r) {
	return r->current_job++;
}

void pak_debug_log(struct PakModule *mod, const char *fmt, ...) {
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

int pak_rt_test_module(struct PakModule *mod) {
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
		if (mod->on_run_test(mod, PAK_SCREEN_CONSOLE, new_job(r))) {
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
