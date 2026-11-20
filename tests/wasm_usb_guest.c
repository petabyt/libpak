#include <stdint.h>
#include <string.h>
#include <libusb.h>
#include <runtime.h>

static int test(struct PakModule *mod, int job) {
	(void)mod; (void)job;
	libusb_context *ctx = NULL;
	libusb_device **list = NULL;
	libusb_device_handle *handle = NULL;
	if (libusb_init(&ctx)) return 1;
	if (libusb_get_device_list(ctx, &list) != 1) return 2;
	if (libusb_open(list[0], &handle)) return 3;
	unsigned char buf[12]; memset(buf, 'X', sizeof(buf));
	if (libusb_get_string_descriptor_ascii(handle, 1, buf, sizeof(buf)) != 5 ||
	    memcmp(buf, "Fuji!\0", 6) || buf[6] != 'X') return 4;
	memset(buf, 'X', sizeof(buf));
	if (libusb_get_string_descriptor_ascii(handle, 1, buf, 3) != 2 || memcmp(buf, "Fu\0X", 4)) return 5;
	if (libusb_get_string_descriptor_ascii(handle, 2, buf, sizeof(buf)) != LIBUSB_ERROR_PIPE) return 6;
	buf[0] = 'X';
	if (libusb_get_string_descriptor_ascii(handle, 3, buf, sizeof(buf)) != 0 || buf[0]) return 9;
	if (libusb_get_string_descriptor_ascii(handle, 1, buf, 0) != LIBUSB_ERROR_INVALID_PARAM) return 7;
	libusb_close(handle);
	if (libusb_get_string_descriptor_ascii(handle, 1, buf, sizeof(buf)) != LIBUSB_ERROR_INVALID_PARAM) return 8;
	libusb_free_device_list(list, 1);
	libusb_exit(ctx);
	return 0;
}
int get_module(struct PakModule *mod) { mod->on_run_test = test; return 0; }
