// Runtime
#include <stdint.h>

/// @addtogroup PakDevice
/// @{
// Photo class
#define PAK_DEVICE_PROFESSIONAL_CAMERA "professional-camera"
#define PAK_DEVICE_ACTION_CAMERA "action-camera"
#define PAK_DEVICE_DASHCAM "dashcam"
#define PAK_DEVICE_SECURITY_CAMERA "security-camera"
#define PAK_DEVICE_BABY_MONITOR "baby-monitor"
#define PAK_DEVICE_GENERIC_CAMERA "generic-camera"
#define PAK_DEVICE_WIFI_SD_CARD "wifi-sd-card"
#define PAK_DEVICE_DOORBELL "doorbell"
// Home class
#define PAK_DEVICE_KITCHEN_APPLIANCE "kitchen-appliance"
#define PAK_DEVICE_GENERIC_APPLIANCE "generic-appliance"
#define PAK_DEVICE_GENERIC_HOME_DEVICE "generic-home-device"
#define PAK_DEVICE_DESK "desk"
#define PAK_DEVICE_GENERIC_FURNITURE "generic-furniture"
#define PAK_DEVICE_WIFI_ROUTER "wifi-router"
#define PAK_DEVICE_3D_PRINTER "3d-printer"
// Accessory class
#define PAK_DEVICE_HEADPHONES "headphones"
#define PAK_DEVICE_EARBUDS "earbuds"
#define PAK_DEVICE_SPEAKERS "speakers"
#define PAK_DEVICE_GENERIC_AUDIO "generic-audio"
#define PAK_DEVICE_SMART_GLASSES "smart-glasses"
#define PAK_DEVICE_GENERIC_TV "generic-tv"
#define PAK_DEVICE_SMARTWATCH "smartwatch"
#define PAK_DEVICE_GENERIC_MEDICAL_WEARABLE "generic-medical-wearable"
#define PAK_DEVICE_GENERIC_EXERCISE_MACHINE "generic-exercise-machine"
// Non-photo gadget class
#define PAK_DEVICE_GAME_CONTROLLER "game-controller"
#define PAK_DEVICE_DRONE "drone"
#define PAK_DEVICE_GENERIC_REMOTE_CONTROL "generic-remote-control"
#define PAK_DEVICE_SCOOTER "scooter"
#define PAK_DEVICE_BICYCLE "bicycle"
#define PAK_DEVICE_GENERIC_RIDEABLE "generic-rideable"
#define PAK_DEVICE_AUTOMOTIVE_INFOTAINMENT "automotive-infotainment"
#define PAK_DEVICE_AUTOMOTIVE_DIAGNOSTIC "automotive-diagnostic"
/// @}

struct Manifest {
	const char *name;
	const char *description;
	const char *author;
	const char *author_url;
	int version;
	int language;
	const char *root_url;
	const char *manifest_path;
	const char *script_path;
	const char *icon_path;

	const char *script_signature;
	const char *script_public_key;

	int primary_discovery_type;

	struct ManifestWiFiDiscovery {
		const char *ssid_pattern;
	}wifi_options;

	struct ManifestBLEDiscovery {
		uint8_t mfg_data_match[0xff];
		uint8_t mfg_data_mask[0xff];
		const char *service_uuid1;
		const char *service_uuid2;
	}ble_options;

	struct ManifestUSBDiscovery {
		int pid;
		int vid;
	}usb_options;

	struct ManifestDatagramDiscovery {
		const char *http_request;
	}upnp_options;

	const char **target_companies;
	const char **target_products;
};
