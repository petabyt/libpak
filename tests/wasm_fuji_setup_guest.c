/* Integration fixture: links libfuji and exercises its real wireless setup. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <runtime.h>
#include <fuji.h>
#include <cl_backend.h>

extern void fuji_ptp_close(struct PtpRuntime *r);

static char camera_name[128];
static int storage_updates;

void app_send_cam_name(struct PtpRuntime *r, const char *name) {
	(void)r;
	snprintf(camera_name, sizeof(camera_name), "%s", name);
}
void app_update_storage_info(struct PtpRuntime *r) { (void)r; storage_updates++; }
void app_print(struct PtpRuntime *r, char *fmt, ...) {
	(void)r;
	va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
	fputc('\n', stderr);
}
int app_check_thread_cancel(struct PtpRuntime *r) { (void)r; return 0; }
int app_queue_file_for_download(struct PtpRuntime *r, int object_id) { (void)r; (void)object_id; return 0; }

static int test(struct PakModule *mod, int job) {
	(void)mod; (void)job;
	struct PtpRuntime *r = fuji_ptp_new(PTP_IP_USB);
	if (!r || !r->priv) return 1;
	r->priv->transport = FUJI_FEATURE_WIRELESS_COMM;
	strcpy(r->priv->ip_address, "127.0.0.1");
	if (ptpip_connect(r, "127.0.0.1", 55740, 1)) return 2;
	int rc = fuji_setup(r, "libpak-test");
	if (rc) { fprintf(stderr, "fuji_setup returned %d\n", rc); return 3; }
	if (!camera_name[0] || !storage_updates) return 4;
	ptp_report_error(r, NULL, 0);
	fuji_ptp_close(r);
	return 0;
}
int get_module(struct PakModule *mod) { mod->on_run_test = test; return 0; }
