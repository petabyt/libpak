/* Deterministic, single-threaded Wi-Fi backend for tests. */
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <wifi.h>

struct PakNet { int connected; };
struct fake_adapter { struct PakWiFiAdapter public; struct PakNet *owner; };
struct fake_ap { struct PakWiFiAp public; struct PakNet *owner; int index; };

static const struct { const char *ssid, *bssid, *password; enum PakWiFiBand band; } aps[] = {
	{ "Fake Open", "02:00:00:00:01:01", NULL, PAK_WIFI_2GHZ },
	{ "Fake Camera", "02:00:00:00:01:02", "camera123", PAK_WIFI_5GHZ },
};
static int valid_adapter(struct PakNet *ctx, struct PakWiFiAdapter *a) {
	return ctx && a && ((struct fake_adapter *)a)->owner == ctx;
}
static int valid_ap(struct PakNet *ctx, struct PakWiFiAp *ap) {
	return ctx && ap && ((struct fake_ap *)ap)->owner == ctx;
}
struct PakNet *pak_net_get_context(void) {
	struct PakNet *ctx = calloc(1, sizeof(*ctx));
	if (ctx) ctx->connected = -1;
	return ctx;
}
void pak_net_unref_context(struct PakNet *ctx) { free(ctx); }
int pak_wifi_is_enabled(struct PakNet *ctx) { return ctx ? 1 : 0; }
int pak_wifi_get_n_adapters(struct PakNet *ctx) { return ctx ? 1 : -1; }
struct PakWiFiAdapter *pak_wifi_get_adapter(struct PakNet *ctx, int index) {
	if (!ctx || (index != 0 && index != -1)) return NULL;
	struct fake_adapter *a = calloc(1, sizeof(*a));
	if (!a) return NULL;
	a->owner = ctx; strcpy(a->public.name, "fake-wifi0"); a->public.is_active = 1;
	return &a->public;
}
int pak_wifi_unref_adapter(struct PakNet *ctx, struct PakWiFiAdapter *a) {
	if (!valid_adapter(ctx, a)) return -1;
	free(a); return 0;
}
int pak_wifi_bind_socket_to_adapter(struct PakNet *ctx, struct PakWiFiAdapter *a, int fd) {
	int type; socklen_t len = sizeof(type);
	if (!valid_adapter(ctx, a) || getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &len)) return PAK_ERR_UNSUPPORTED;
	return 0;
}
int pak_wifi_request_scan(struct PakNet *ctx, struct PakWiFiAdapter *a) { return valid_adapter(ctx, a) ? 0 : -1; }
int pak_wifi_get_n_aps(struct PakNet *ctx, struct PakWiFiAdapter *a) {
	return valid_adapter(ctx, a) ? (int)(sizeof(aps) / sizeof(aps[0])) : -1;
}
struct PakWiFiAp *pak_wifi_get_ap(struct PakNet *ctx, struct PakWiFiAdapter *a, int index) {
	if (!valid_adapter(ctx, a) || index < 0 || index >= (int)(sizeof(aps) / sizeof(aps[0]))) return NULL;
	struct fake_ap *ap = calloc(1, sizeof(*ap));
	if (!ap) return NULL;
	ap->owner = ctx; ap->index = index;
	strcpy(ap->public.ssid, aps[index].ssid); strcpy(ap->public.bssid, aps[index].bssid);
	ap->public.band = aps[index].band;
	return &ap->public;
}
int pak_wifi_unref_ap(struct PakNet *ctx, struct PakWiFiAdapter *a, struct PakWiFiAp *ap) {
	if (!valid_adapter(ctx, a) || !valid_ap(ctx, ap)) return -1;
	free(ap); return 0;
}
struct PakWiFiAp *pak_wifi_get_connected_ap(struct PakNet *ctx, struct PakWiFiAdapter *a) {
	if (!valid_adapter(ctx, a) || ctx->connected < 0) return NULL;
	return pak_wifi_get_ap(ctx, a, ctx->connected);
}
int pak_wifi_connect_to_ap(struct PakNet *ctx, struct PakWiFiAdapter *a, struct PakWiFiAp *ap,
		const char *password, int blocking) {
	(void)blocking;
	if (!valid_adapter(ctx, a) || !valid_ap(ctx, ap)) return -1;
	int index = ((struct fake_ap *)ap)->index;
	if (aps[index].password && (!password || strcmp(password, aps[index].password))) return PAK_ERR_NO_CONNECTION;
	ctx->connected = index;
	return 0;
}
