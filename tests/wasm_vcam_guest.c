#include <stdint.h>
#include <libusb.h>
#include <runtime.h>

static unsigned u16(const unsigned char *p) { return (unsigned)p[0] | (unsigned)p[1] << 8; }
static unsigned u32(const unsigned char *p) { return u16(p) | u16(p + 2) << 16; }

static int test(struct PakModule *mod, int job) {
	(void)mod; (void)job;
	libusb_context *ctx = NULL;
	libusb_device **list = NULL;
	libusb_device_handle *handle = NULL;
	struct libusb_device_descriptor device;
	struct libusb_config_descriptor *config = NULL;
	if (libusb_init(&ctx)) return 1;
	if (libusb_get_device_list(ctx, &list) != 1) return 2;
	if (libusb_get_device_descriptor(list[0], &device) || device.idVendor != 0x04cb) return 3;
	if (libusb_get_config_descriptor(list[0], 0, &config)) return 4;
	if (!config->bNumInterfaces || !config->interface || !config->interface[0].num_altsetting) return 5;
	const struct libusb_interface_descriptor *iface = config->interface[0].altsetting;
	unsigned char in = 0, out = 0;
	for (int i = 0; i < iface->bNumEndpoints; i++) {
		const struct libusb_endpoint_descriptor *ep = &iface->endpoint[i];
		if ((ep->bmAttributes & 3) != LIBUSB_TRANSFER_TYPE_BULK) continue;
		if (ep->bEndpointAddress & LIBUSB_ENDPOINT_IN) in = ep->bEndpointAddress;
		else out = ep->bEndpointAddress;
	}
	int interface = iface->bInterfaceNumber;
	libusb_free_config_descriptor(config);
	if (!in || !out || libusb_open(list[0], &handle)) return 6;
	if (libusb_claim_interface(handle, interface)) return 7;
	/* PTP GetDeviceInfo, transaction 0. */
	unsigned char command[12] = {12, 0, 0, 0, 1, 0, 1, 0x10, 0, 0, 0, 0};
	unsigned char response[4096]; int transferred = 0;
	if (libusb_bulk_transfer(handle, out, command, sizeof(command), &transferred, 1000) || transferred != 12) return 8;
	if (libusb_bulk_transfer(handle, in, response, sizeof(response), &transferred, 1000)) return 9;
	if (transferred < 12 || u32(response) < 12 || u16(response + 4) != 2 || u16(response + 6) != 0x1001) return 10;
	if (libusb_bulk_transfer(handle, in, response, sizeof(response), &transferred, 1000)) return 11;
	if (transferred < 12 || u16(response + 4) != 3 || u16(response + 6) != 0x2001) return 12;
	libusb_release_interface(handle, interface);
	libusb_close(handle);
	libusb_free_device_list(list, 1);
	libusb_exit(ctx);
	return 0;
}
int get_module(struct PakModule *mod) { mod->on_run_test = test; return 0; }
