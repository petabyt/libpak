#include <assert.h>
#include <string.h>
#include <wifi.h>

int main(void) {
	struct PakNet *net = pak_net_get_context(); assert(net && pak_wifi_is_enabled(net));
	assert(pak_wifi_get_n_adapters(net) == 1 && !pak_wifi_get_adapter(net, 1));
	struct PakWiFiAdapter *adapter = pak_wifi_get_adapter(net, -1); assert(adapter);
	assert(!strcmp(adapter->name, "fake-wifi0") && adapter->is_active);
	assert(!pak_wifi_request_scan(net, adapter) && pak_wifi_get_n_aps(net, adapter) == 2);
	assert(!pak_wifi_get_connected_ap(net, adapter));
	assert(!pak_wifi_get_ap(net, adapter, -1) && !pak_wifi_get_ap(net, adapter, 2));
	struct PakWiFiAp *ap = pak_wifi_get_ap(net, adapter, 1); assert(ap);
	assert(!strcmp(ap->ssid, "Fake Camera") && ap->band == PAK_WIFI_5GHZ);
	assert(pak_wifi_connect_to_ap(net, adapter, ap, "wrong", PAK_BLOCK) == PAK_ERR_NO_CONNECTION);
	assert(!pak_wifi_get_connected_ap(net, adapter));
	assert(!pak_wifi_connect_to_ap(net, adapter, ap, "camera123", PAK_NONBLOCK));
	struct PakWiFiAp *connected = pak_wifi_get_connected_ap(net, adapter); assert(connected);
	assert(!strcmp(connected->bssid, ap->bssid));
	assert(pak_wifi_connect_to_ap(net, adapter, ap, NULL, PAK_BLOCK) == PAK_ERR_NO_CONNECTION);
	assert(pak_wifi_bind_socket_to_adapter(net, adapter, 0) == PAK_ERR_UNSUPPORTED);
	assert(!pak_wifi_unref_ap(net, adapter, connected));
	assert(!pak_wifi_unref_ap(net, adapter, ap));
	struct PakWiFiAp *open = pak_wifi_get_ap(net, adapter, 0); assert(open);
	assert(!pak_wifi_connect_to_ap(net, adapter, open, NULL, PAK_BLOCK));
	connected = pak_wifi_get_connected_ap(net, adapter); assert(connected && !strcmp(connected->ssid, "Fake Open"));
	assert(!pak_wifi_unref_ap(net, adapter, connected));
	assert(!pak_wifi_unref_ap(net, adapter, open));
	assert(!pak_wifi_unref_adapter(net, adapter));
	pak_net_unref_context(net);
	return 0;
}
