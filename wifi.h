#pragma once
#include <stdint.h>
struct WiFiContext;

struct WiFiDeviceList {
	int32_t length;
	struct WiFiDevice {
		char name[32];
		int is_active;
		void *priv;
	}list[];
};

struct WiFiApList {
	int32_t length;
	struct WiFiAp {
		char ssid[33];
		char bssid[6];
		void *priv;
	}list[];
};
