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

__attribute__((weak))
void pak_global_log(const char *fmt, ...) {
	printf("LOG: ");
	fflush(stdout);
	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	putchar('\n');
}

__attribute__((weak))
void pak_error(const char *fmt, ...) {
	printf("ERR: ");
	fflush(stderr);
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

__attribute__((weak))
void pak_abort(const char *fmt, ...) {
	printf("ABORT: ");
	fflush(stdout);
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	fflush(stdout);
	abort();
}

int pak_rt_add_wifi_connection(struct PakModule *mod, struct PakWiFiApFilter *filter, const char *setup_option) {
	return -1;
}
int pak_rt_set_download_stats(struct PakModule *mod, int job, long time, unsigned int n_bytes) {
	return -1;
}
int pak_rt_set_widget(struct PakModule *mod, const struct PakWidget *s) {
	return 0;
}
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
int pak_rt_add_file_contents(struct PakModule *mod, struct PakFileHandle *file, void *image_data, unsigned int length, uint64_t offset, uint64_t total_size) {
	return -1;
}
int pak_rt_set_storage_info(struct PakModule *mod, const char *name, struct PakStorageInfo *info) {
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
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	putchar('\n');
	fflush(stdout);
}

int pak_rt_test_module(struct PakModule *mod) {
	// runtime was not inited by pak_create_mod
	mod->rt = malloc(sizeof(struct RuntimePriv));
	mod->rt->current_job = 1;

	struct RuntimePriv *r = mod->rt;
	if (mod->init) mod->init(mod);
	if (mod->on_run_test) {
		if (mod->on_run_test(mod, new_job(r))) {
			printf("on_run_test\n");
			return -1;
		}
	}
	return 0;
}
