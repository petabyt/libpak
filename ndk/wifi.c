#include <jni.h>
#include <wifi.h>

int pak_wifi_get_n_adapters(struct PakNet *ctx) {

}
int pak_wifi_get_adapter(struct PakNet *ctx, struct PakWiFiAdapter *adapter, int index) {

}
int pak_wifi_unref_adapter(struct PakNet *ctx, struct PakWiFiAdapter *adapter) {

}

/// Bind a socket file descriptor to a network adapter
int pak_wifi_bind_socket_to_adapter(struct PakNet *ctx, struct PakWiFiAdapter *adapter, int fd) {

}

/// Request the adapter to perform a scan (roam)
int pak_wifi_request_scan(struct PakNet *ctx, struct PakWiFiAdapter *adapter) {

}

int pak_wifi_get_n_aps(struct PakNet *ctx, struct PakWiFiAdapter *adapter) {
	return -1;
}
int pak_wifi_get_ap(struct PakNet *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap, int index) {
	return -1;
}
int pak_wifi_unref_ap(struct PakNet *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap) {
	return -1;
}

int pak_wifi_get_connected_ap(struct PakNet *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap) {
	return -1;
}

int pak_wifi_connect_to_ap(struct PakNet *ctx, struct PakWiFiAdapter *adapter, struct PakWiFiAp *ap, const char *password) {
	return -1;
}

int pak_wifi_request_connection(struct PakNet *ctx, struct PakWiFiApFilter *spec, int (*cb)(struct PakNet *ctx, struct PakWiFiAdapter *, void *arg), void *arg) {
	return -1;
}

JNIEXPORT void JNICALL
Java_dev_danielc_libpak_WiFi_00024NativeWiFiDiscoveryCallback_found(JNIEnv *env, jobject thiz,
																	jobject net) {
	// TODO: implement found()
}

JNIEXPORT void JNICALL
Java_dev_danielc_libpak_WiFi_00024NativeWiFiDiscoveryCallback_failed(JNIEnv *env, jobject thiz,
																	 jstring reason, jint code) {
	// TODO: implement failed()
}