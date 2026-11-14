#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <runtime.h>
#include <wifi.h>
#include "dummyjpg.h"
#include "dummythumb.h"

struct ModulePriv {
	int x;
	int is_slow;
	int is_live;
};

static int on_find_connection(struct PakModule *mod, int job) {
	pak_debug_log(mod, "Connection established");
	if (mod->priv->is_slow) {
		for (int i = 0; i < 10; i++) {
			if (pak_rt_is_job_cancelled(mod, job)) return PAK_ERR_CANCELLED;
			pak_rt_set_progress_bar(mod, job, i * 10);
			usleep(100000);
		}
	}

	return 0;
}

static int init(struct PakModule *mod) {
	pak_debug_log(mod, "Hello from dummy module");
	mod->priv = (struct ModulePriv *)calloc(sizeof(struct ModulePriv), 1);
	pak_rt_set_session_property(mod, PAK_PROP_NAME, "Dummy Device");
	pak_rt_set_session_property(mod, PAK_PROP_FW_VER, "v1.2.3");

	const char *setup_option = pak_rt_get_setup_option(mod) == NULL ? "none" : pak_rt_get_setup_option(mod);

	if (!strcmp(setup_option, "slow") || !strcmp(setup_option, "camera")) {
		if (!strcmp(setup_option, "slow")) mod->priv->is_slow = 1;
		pak_rt_set_screen_supported(mod, PAK_SCREEN_DASHBOARD, 1);
		pak_rt_set_screen_supported(mod, PAK_SCREEN_FILE_GALLERY, 1);
		pak_rt_set_screen_supported(mod, PAK_SCREEN_FILE_VIEWER, 1);
		pak_rt_set_screen_supported(mod, PAK_SCREEN_LIVEVIEW, 1);
		pak_rt_set_screen_supported(mod, PAK_SCREEN_INTERVALOMETER, 1);
		pak_rt_set_storage_info(mod, "sdcard", &(struct PakStorageInfo){
			.sorted_by = PAK_NEWEST_FIRST,
			.n_files_root = 10,
		});
		pak_rt_set_storage_info(mod, "sdcard2", &(struct PakStorageInfo){
			.sorted_by = PAK_NEWEST_FIRST,
			.n_files_root = 1000,
		});
		pak_rt_set_widget(mod, "button", &(struct PakWidget) {
				.title = "A Button",
				.type = PAK_BUTTON,
		});

		pak_rt_set_widget(mod, "bool", &(struct PakWidget) {
				.title = "A Switch",
				.type = PAK_BOOLEAN,
				.u.boolv.v = 1,
		});

		pak_rt_set_widget(mod, "dropdown", &(struct PakWidget) {
				.title = "Option",
				.type = PAK_COMBOBOX,
				.u.comboboxv = {
						.index_value = 1,
						.list = (const char *[]) {"4.0l I6", "5.6l v8", "7.4l v8", "2.8l tdi", NULL}
				}
		});

		pak_rt_set_widget(mod, "iso", &(struct PakWidget) {
				.title = "ISO",
				.group = PAK_GROUP_LIVEVIEW,
				.type = PAK_COMBOBOX,
				.u.comboboxv = {
						.index_value = -1,
						.list = (const char *[]) {"6400", "3200", "1600", "800", "600", "400", "200", "100", NULL}
				}
		});
		pak_rt_set_widget(mod, "shutter-speed", &(struct PakWidget) {
				.title = "Shutter Speed",
				.group = PAK_GROUP_LIVEVIEW,
				.type = PAK_COMBOBOX,
				.u.comboboxv = {
						.index_value = -1,
						.list = (const char *[]) {"1/8000", "1/4000", "1/2000", "1/1000", "1/500", "1/250", "1/125", "1/60", "1/30", "1/15", "1/8", "1/4", "1/2", "1\"", "2\"", "4\"", "8\"", "15\"", "30\"", NULL}
				}
		});
		pak_rt_set_widget(mod, "aperture", &(struct PakWidget) {
				.title = "Aperture",
				.group = PAK_GROUP_LIVEVIEW,
				.type = PAK_COMBOBOX,
				.u.comboboxv = {
						.index_value = -1,
						.list = (const char *[]) {"f/1.0", "f/1.4", "f/2", "f/2.8", "f/4", "f/5.6", "f/8", "f/11", "f/16", "f/22", "f/32", NULL}
				}
		});
	} else if (!strcmp(setup_option, "tethered")) {
		pak_rt_set_screen_supported(mod, PAK_SCREEN_DASHBOARD, 1);
		pak_rt_set_screen_supported(mod, PAK_SCREEN_LIVE_FEED, 1);
		pak_rt_set_storage_info(mod, "live", &(struct PakStorageInfo){
			.n_files_root = 1,
			.type = PAK_LIVE_QUEUE,
		});
		mod->priv->is_live = 1;
		pak_rt_set_widget(mod, "img", &(struct PakWidget) {
			.title = "Trigger capture",
			.type = PAK_BUTTON,
		});
	}

	pak_rt_set_tick_interval(mod, 1000 * 200);
	return 0;
}

static int on_idle_tick(struct PakModule *mod, unsigned int us_since_last_tick) {
	pak_rt_set_session_property_int(mod, PAK_PROP_BATTERY_MAIN, mod->priv->x);
	if ((mod->priv->x += 10) > 100) mod->priv->x = 0;
	return 0;
}

static int on_disconnect(struct PakModule *mod) {
	return 0;
}

static int on_switch_screen(struct PakModule *mod, int old_screen, int new_screen, int job) {
	pak_debug_log(mod, "dummymod: Switching screen (%d -> %d)", old_screen, new_screen);
	if (mod->priv->is_slow) {
		if (new_screen == PAK_SCREEN_FILE_VIEWER) {
			for (int i = 0; i < 100; i++) {
				if (pak_rt_is_job_cancelled(mod, job)) return 0;
				pak_rt_set_progress_bar(mod, job, i);
				usleep(10000);
			}
		}
	}
	return 0;
}

static int on_request_file_contents(struct PakModule *mod, int job, struct PakObjectHandle *file) {
#define min(a, b)  ((a) < (b) ? (a) : (b))
	unsigned int of = 0;
	for (int i = 0; i < 100; i++) {
		if (pak_rt_is_job_cancelled(mod, job)) return 0;
		unsigned int len = min(sizeof(_dummy_jpeg_jpg) / 100, sizeof(_dummy_jpeg_jpg) - of);
		pak_rt_add_file_contents(mod, file, _dummy_jpeg_jpg + of, len, of, sizeof(_dummy_jpeg_jpg));
		of += len;
		pak_rt_set_progress_bar(mod, job, i);
		usleep(10000);
	}
	pak_rt_add_file_contents(mod, file, _dummy_jpeg_jpg + of, sizeof(_dummy_jpeg_jpg) - of, of, sizeof(_dummy_jpeg_jpg));
	return 0;
}

static int on_request_thumbnail(struct PakModule *mod, int job, struct PakObjectHandle *file) {
	usleep(100000);
	return pak_rt_add_file_thumbnail(mod, file, _dummy_thumb_jpg, sizeof(_dummy_thumb_jpg));
}

static int on_request_file_metadata(struct PakModule *mod, int job, struct PakObjectHandle *file) {
	usleep(100000);
	char name[32];
	const char *prefix = "ABCD";
	if (!strcmp(file->storage_name, "sdcard2")) prefix = "EFGH";
	sprintf(name, "%s%04u.JPG", prefix, file->index_in_view * 13);
	pak_rt_add_file_metadata(mod, file, &(struct PakFileMetadata){
		.filename = name,
		.file_size = 123,
		.mime_type = "image/jpeg",
	});
	return 0;
}

static int on_request_liveview_frame(struct PakModule *mod, int job, struct PakObjectHandle *handle) {
	return pak_rt_add_file_contents(mod, handle, _dummy_jpeg_jpg, sizeof(_dummy_jpeg_jpg), 0, 0);
}

static int on_run_test(struct PakModule *mod, int screen, int job) {
	return 0;
}

static int on_custom_command(struct PakModule *mod, int job, int argc, const char * const *argv) {
	if (!strcmp(argv[0], "help")) {
		pak_global_log("Foo fighters");
	}
	return 0;
}

static int on_prop_changed(struct PakModule *mod, int job, const char *name, struct PakWidget *prop) {
	pak_global_log("on_prop_changed %s", name);
	if (!strcmp(name, "img")) {
		static int n_downloaded = 0;
		pak_rt_add_file_metadata(mod, &(struct PakObjectHandle){.index_in_view = n_downloaded++, .storage_name = "live"}, &(struct PakFileMetadata){
			.filename = "DSCF1001.JPG",
			.mime_type = "image/jpeg",
		});
	}
	return 0;
}

int get_module(struct PakModule *mod) {
	mod->init = init;
	mod->on_request_file_thumbnail = on_request_thumbnail;
	mod->on_request_object_info = on_request_file_metadata;
	mod->on_request_liveview_frame = on_request_liveview_frame;
	mod->on_request_file_contents = on_request_file_contents;
	mod->on_find_connection = on_find_connection;
	mod->on_idle_tick = on_idle_tick;
	mod->on_disconnect = on_disconnect;
	mod->on_switch_screen = on_switch_screen;
	mod->on_custom_command = on_custom_command;
	mod->on_widget_updated = on_prop_changed;
	return 0;
}
