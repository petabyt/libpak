#include <stddef.h>
#include <string.h>
#include <runtime.h>

_Static_assert(sizeof(struct PakWiFiAdapter) == 40 && offsetof(struct PakWiFiAdapter, is_active) == 36, "wasm32 adapter ABI");
_Static_assert(sizeof(struct PakWiFiAp) == 76 && offsetof(struct PakWiFiAp, band) == 72, "wasm32 AP ABI");

static int test(struct PakModule *mod, int job) {
	(void)job;
	struct PakNet *ctx = pak_net_get_context();
	if (!ctx || (mod->net && mod->net != ctx) || !pak_wifi_is_enabled(ctx)) return 1;
	if (pak_wifi_get_n_adapters(ctx) != 1 || pak_wifi_get_adapter(ctx, 1)) return 2;
	struct PakWiFiAdapter *a = pak_wifi_get_adapter(ctx, 0);
	struct PakWiFiAdapter *other = pak_wifi_get_adapter(ctx, -1);
	if (!a || !other || strcmp(a->name, "fake-wifi0") || !a->is_active) return 3;
	if (pak_wifi_request_scan(ctx, a) || pak_wifi_get_n_aps(ctx, a) != 2) return 4;
	if (pak_wifi_get_connected_ap(ctx, a) || pak_wifi_get_ap(ctx, a, 2)) return 5;
	struct PakWiFiAp *ap = pak_wifi_get_ap(ctx, a, 1);
	if (!ap || strcmp(ap->ssid, "Fake Camera") || ap->band != PAK_WIFI_5GHZ || strcmp(ap->bssid, "02:00:00:00:01:02")) return 6;
	if (pak_wifi_connect_to_ap(ctx, a, ap, "wrong", PAK_BLOCK) != PAK_ERR_NO_CONNECTION) return 7;
	if (pak_wifi_get_connected_ap(ctx, a)) return 8;
	if (!pak_wifi_connect_to_ap(ctx, other, ap, "camera123", PAK_BLOCK)) return 9;
	if (pak_wifi_connect_to_ap(ctx, a, ap, "camera123", PAK_BLOCK)) return 10;
	struct PakWiFiAp *current = pak_wifi_get_connected_ap(ctx, a);
	if (!current || strcmp(current->ssid, ap->ssid)) return 11;
	if (pak_wifi_bind_socket_to_adapter(ctx, a, 1) != PAK_ERR_UNSUPPORTED) return 12;
	if (!pak_wifi_unref_ap(ctx, other, ap)) return 13;
	if (pak_wifi_unref_ap(ctx, a, current)) return 14;
	if (pak_wifi_unref_adapter(ctx, other)) return 15;
	/* An AP keeps the native parent alive even if the guest releases it first. */
	if (pak_wifi_unref_adapter(ctx, a)) return 16;
	if (pak_wifi_unref_ap(ctx, a, ap)) return 17;
	if (!pak_wifi_unref_ap(ctx, a, ap)) return 18;
	/* Leave a live AP and a released parent for instance teardown. */
	a = pak_wifi_get_adapter(ctx, 0); if (!a) return 19;
	ap = pak_wifi_get_ap(ctx, a, 0); if (!ap) return 20;
	if (pak_wifi_unref_adapter(ctx, a)) return 21;
	pak_net_unref_context(ctx);
	return 0;
}
int get_module(struct PakModule *mod) { mod->on_run_test = test; return 0; }
