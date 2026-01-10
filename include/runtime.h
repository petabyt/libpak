// Runtime
#include <stdint.h>
#include <wifi.h>
#include <bluetooth.h>

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

struct FileMetadata {
	const char *filename;
	const char *mime_type;
};

struct Module {
	enum Screen {
		SCREEN_CONNECT_WIFI = 1,
		SCREEN_CONNECT_USB,
		SCREEN_CONNECT_BLUETOOTH,
		SCREEN_ADVERTISE_BLUETOOTH,
		SCREEN_TRANSMIT_UDP,
		SCREEN_RECEIVE_UDP,
		SCREEN_TEST_SUITE,

		SCREEN_FILE_GALLERY,
		SCREEN_FILE_VIEWER,
		SCREEN_GEOTAGGING,
		SCREEN_LIVEVIEW,
		SCREEN_LIVE_FEED,
	}default_screen;

	int version;
	struct ModulePriv *priv;
	int (*get_manifest)(struct Module *);
	int (*init)(struct Module *);
	/// Try to initiate a connection over a network handle
	int (*on_try_connect_wifi)(struct Module *, struct PakWiFiAdapter *handle, int job);
	/// Runs immediately after successful connection. Runs at a constant interval, 1s by default.
	int (*on_idle_tick)(struct Module *, unsigned int us_since_last_tick);
	/// On user requested disconnect
	int (*on_disconnect)(struct Module *);
	/// Runs when a new screen is entered or returned to. This doesn't ask for any new data, it only notifies the module
	/// to switch states for a different set of commands
	int (*on_enter_screen)(struct Module *, int screen, int job);
	/// When a screen is exited. When the previous screen is being entered back into, on_enter_screen will be called
	/// TODO: switch to int (*on_switch_screen)(struct Module *, int old_screen, int new_screen, int job); 
	int (*on_exit_screen)(struct Module *, int screen, int job);
	/// 
	int (*on_request_file_contents)(struct Module *, int screen, int job, void *file_tag);
	/// Process an arbritrary command (from a console)
	int (*on_custom_command)(struct Module *, const char *request);
};

int pak_mod_add_file_thumbnail(struct Module *mod, void *file_tag, void *image_data, unsigned int length);
int pak_mod_add_file_metadata(struct Module *mod, void *file_tag, struct FileMetadata *metadata);
int pak_mod_add_file_contents(struct Module *mod, void *file_tag, void *image_data, unsigned int length);

int pak_mod_enter_screen(struct Module *mod, int screen);
int pak_mod_enter_custom_screen(struct Module *mod);
int pak_mod_set_progress_bar(struct Module *mod, int job, int percent);
int pak_mod_set_current_download_speed(struct Module *mod, long time, unsigned int n_bytes);
int pak_mod_set_device_name(struct Module *mod, const char *name);
int pak_mod_set_device_unique_id(struct Module *mod, const char *string);
int pak_mod_load_device_unique_id(struct Module *mod, const char *string);
int pak_mod_flip_kill_switch(struct Module *mod, const char *reason);
int pak_mod_set_tick_interval(struct Module *mod, unsigned int us);
const char *get_path(struct Module *mod, const char *filename);

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

	struct TargetInfo {
		const char *device_type;
		const char **companies;
		const char **products;
	}target;

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
		int pid_mask;
		int pid;
		int vid_mask;
		int vid;
		int class_mask;
		int class_;
	}usb_options;

	struct ManifestDatagramDiscovery {
		const char *http_request;
	}upnp_options;
};
