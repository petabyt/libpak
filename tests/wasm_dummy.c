#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <runtime.h>
#include <runtime_ext.h>
#include "../src/wasm/memory.h"

static const char *option;
static int properties, screens, storages, widgets, metadata, thumbnails, contents, battery;
static uint64_t bytes;

const char *pak_rt_get_setup_option(struct PakModule *m) { (void)m; return option; }
int pak_rt_set_session_property(struct PakModule *m, const char *key, const char *value) {
	(void)m; assert(key && value); properties++; return 0;
}
int pak_rt_set_session_property_int(struct PakModule *m, const char *key, int value) {
	(void)m; assert(!strcmp(key, PAK_PROP_BATTERY_MAIN)); battery = value + 1; return 0;
}
int pak_rt_set_screen_supported(struct PakModule *m, int screen, int value) {
	(void)m; assert(screen && value); screens++; return 0;
}
int pak_rt_set_storage_info(struct PakModule *m, const char *name, struct PakStorageInfo *s) {
	(void)m; assert(name && s && s->n_files_root); storages++; return 0;
}
int pak_rt_set_widget(struct PakModule *m, const char *name, const struct PakWidget *w) {
	(void)m; assert(name && w && w->title); widgets++;
	if (w->type == PAK_COMBOBOX) assert(w->u.comboboxv.list && w->u.comboboxv.list[0]);
	return 0;
}
int pak_rt_set_tick_interval(struct PakModule *m, unsigned int us) { (void)m; assert(us == 200000); return 0; }
int pak_rt_is_job_cancelled(struct PakModule *m, int job) { (void)m; (void)job; return 0; }
int pak_rt_set_progress_bar(struct PakModule *m, int job, int percent) { (void)m; (void)job; (void)percent; return 0; }
int pak_rt_add_file_metadata(struct PakModule *m, struct PakObjectHandle *file, const struct PakFileMetadata *md) {
	(void)m; assert(file && md && md->filename && md->mime_type); metadata++; return 0;
}
int pak_rt_add_file_thumbnail(struct PakModule *m, struct PakObjectHandle *file, const void *data, unsigned int len) {
	(void)m; assert(file && data && len); thumbnails++; return 0;
}
int pak_rt_add_file_contents(struct PakModule *m, struct PakObjectHandle *file, const void *data, unsigned int len, uint64_t offset, uint64_t total) {
	(void)m; assert(file && data && len); assert(!total || offset + len <= total); contents++; bytes += len; return 0;
}

int main(int argc, char **argv) {
	uint8_t raw[] = { 0xff, 0x78, 0x56, 0x34, 0x12, 0xef, 0xcd, 0xab, 0x90 };
	assert(wasm_read_u32le(raw + 1) == UINT32_C(0x12345678));
	assert(wasm_read_u64le(raw + 1) == UINT64_C(0x90abcdef12345678));
	wasm_put_u32le(raw + 1, UINT32_C(0x89abcdef));
	assert(raw[1] == 0xef && raw[2] == 0xcd && raw[3] == 0xab && raw[4] == 0x89);
	wasm_put_u16le(raw + 1, 0x1234);
	assert(raw[1] == 0x34 && raw[2] == 0x12);
	assert(argc == 2);
	FILE *f = fopen(argv[1], "rb"); assert(f);
	assert(!fseek(f, 0, SEEK_END)); long len = ftell(f); assert(len > 0);
	rewind(f); char *buf = malloc((size_t)len); assert(buf);
	assert(fread(buf, 1, (size_t)len, f) == (size_t)len); fclose(f);
	struct PakModule m = {0};
	assert(setup_wasm_module(&m, "bad", 3) != 0);
	assert(setup_wasm_module(&m, buf, (unsigned)len) == 0);
	struct PakModule second = {0};
	assert(setup_wasm_module(&second, buf, (unsigned)len) == 0);
	free(buf); /* the loaded module owns its copy */
	option = "camera";
	assert(m.init && !m.init(&m));
	assert(properties == 2 && screens == 5 && storages == 2 && widgets == 6);
	assert(m.on_find_connection && !m.on_find_connection(&m, 1));
	assert(m.on_idle_tick && !m.on_idle_tick(&m, 1000) && battery == 1);
	assert(m.on_switch_screen && !m.on_switch_screen(&m, 1, 2, 1));
	struct PakObjectHandle h = { .index_in_view = 2, .storage_name = "sdcard2", .path = "DCIM" };
	assert(m.on_request_object_info && !m.on_request_object_info(&m, 1, &h) && metadata == 1);
	assert(m.on_request_file_thumbnail && !m.on_request_file_thumbnail(&m, 1, &h) && thumbnails == 1);
	assert(m.on_request_liveview_frame && !m.on_request_liveview_frame(&m, 1, &h) && contents == 1 && bytes);
	uint64_t frame_size = bytes;
	assert(m.on_request_file_contents && !m.on_request_file_contents(&m, 1, &h));
	assert(contents == 102 && bytes == frame_size * 2);
	const char *args[] = { "help" };
	assert(m.on_custom_command && !m.on_custom_command(&m, 1, 1, args));
	struct PakWidget w = { .type = PAK_BUTTON, .title = "capture" };
	assert(m.on_widget_updated && !m.on_widget_updated(&m, 1, "img", &w) && metadata == 2);
	assert(m.on_disconnect && !m.on_disconnect(&m));
	assert(m.free && !m.free(&m));
	assert(!m.init && !m.free);
	option = "tethered";
	assert(second.init && !second.init(&second));
	assert(properties == 4 && screens == 7 && storages == 3 && widgets == 7);
	assert(second.free && !second.free(&second));
	return 0;
}
