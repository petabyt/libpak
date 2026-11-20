#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

static int context, device, handle;
int libusb_init(libusb_context **ctx) { if (ctx) *ctx = (libusb_context *)&context; return 0; }
void libusb_exit(libusb_context *ctx) { (void)ctx; }
ssize_t libusb_get_device_list(libusb_context *ctx, libusb_device ***list) {
	(void)ctx;
	*list = calloc(2, sizeof(**list));
	if (!*list) return LIBUSB_ERROR_NO_MEM;
	(*list)[0] = (libusb_device *)&device;
	return 1;
}
void libusb_free_device_list(libusb_device **list, int unref) { (void)unref; free(list); }
libusb_device *libusb_ref_device(libusb_device *dev) { return dev; }
void libusb_unref_device(libusb_device *dev) { (void)dev; }
int libusb_open(libusb_device *dev, libusb_device_handle **out) {
	if (dev != (libusb_device *)&device) return LIBUSB_ERROR_NO_DEVICE;
	*out = (libusb_device_handle *)&handle;
	return 0;
}
void libusb_close(libusb_device_handle *dev) { (void)dev; }
int libusb_get_string_descriptor_ascii(libusb_device_handle *dev, uint8_t index, unsigned char *data, int length) {
	if (dev != (libusb_device_handle *)&handle) abort();
	if (index == 3) { data[0] = 0; return 0; }
	if (index != 1) return LIBUSB_ERROR_PIPE;
	int n = length - 1 < 5 ? length - 1 : 5;
	memcpy(data, "Fuji!", (size_t)n); data[n] = 0;
	return n;
}
