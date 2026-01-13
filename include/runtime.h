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
//#define PAK_DEVICE_SECURITY_CAMERA "security-camera"
//#define PAK_DEVICE_BABY_MONITOR "baby-monitor"
#define PAK_DEVICE_GENERIC_CAMERA "generic-camera"
#define PAK_DEVICE_WIFI_SD_CARD "wifi-sd-card"
#define PAK_DEVICE_DOORBELL "doorbell"
// Home class
//#define PAK_DEVICE_KITCHEN_APPLIANCE "kitchen-appliance"
//#define PAK_DEVICE_GENERIC_APPLIANCE "generic-appliance"
#define PAK_DEVICE_GENERIC_HOME_DEVICE "generic-home-device"
#define PAK_DEVICE_DESK "desk"
#define PAK_DEVICE_GENERIC_FURNITURE "generic-furniture"
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
#define PAK_DEVICE_POWER_TOOL "power-tool"
#define PAK_DEVICE_GAME_CONTROLLER "game-controller"
#define PAK_DEVICE_DRONE "drone"
#define PAK_DEVICE_GENERIC_REMOTE_CONTROL "generic-remote-control"
#define PAK_DEVICE_SCOOTER "scooter"
#define PAK_DEVICE_BICYCLE "bicycle"
#define PAK_DEVICE_GENERIC_RIDEABLE "generic-rideable"
#define PAK_DEVICE_AUTOMOTIVE_INFOTAINMENT "automotive-infotainment"
#define PAK_DEVICE_AUTOMOTIVE_DIAGNOSTIC "automotive-diagnostic"
/// @}

struct FileHandle {
	int index_in_view;
	const char *filename;
};

struct FileMetadata {
	const char *filename;
	const char *mime_type;
};

enum Screen {
	/// A screen that has a list of WiFi access points to connect to
	SCREEN_CONNECT_WIFI = 1,
	/// A screen that has a list of connected USB devices
	SCREEN_CONNECT_USB,
	/// Has a list of currently/previously paired devices and scans for new devices
	SCREEN_CONNECT_BLUETOOTH,
	/// Screen that advertises on BLE or uses SDP to find a device to pair with
	SCREEN_ADVERTISE_BLUETOOTH,
	/// Transmit packet over UDP/SSDP in order to find a connection
	SCREEN_TRANSMIT_UDP,
	/// Receive packet over UDP/SSDP in order to find a connection
	SCREEN_RECEIVE_UDP,
	/// An option to test various functionality in this module using a fake emulator
	SCREEN_TEST_SUITE,

	/// A gallery of files, videos, or photos. Can include folders. Upon selecting a folder, on_switch_screen
	/// will be called with SCREEN_FILE_GALLERY->SCREEN_FILE_GALLERY
	/// Gallery may be a table of files with detailed info, or a thumbnail gallery of variable width.
	/// In what order the files iterated through (LIFO/FIFO) can be controlled.
	SCREEN_FILE_GALLERY,
	/// A zoomable image viewer or video player. User may swipe left or right to view next/previous file. When this happens,
	/// on_switch_screen will be called with SCREEN_FILE_VIEWER->SCREEN_FILE_VIEWER
	SCREEN_FILE_VIEWER,
	/// Can be used to send location data to the camera or apply location metadata to a specific photo.
	SCREEN_GEOTAGGING,
	/// A constant live view of the camera's sensor. Allows setting various settings such as ISO, aperture, exposure settings, image settings, or video settings.
	/// Allows recording video, taking photos, controlling focus, zoom, or SLR mirror.
	SCREEN_LIVEVIEW,
	/// A feed of incoming files that are being created/captured/sent by the device.
	SCREEN_LIVE_FEED,
};

struct Module {
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
	// Runs when switching to a new screen, or switching back to an old screen.
	int (*on_switch_screen)(struct Module *, int old_screen, int new_screen, int job);
	/// Request entire contents of a file
	int (*on_request_file_contents)(struct Module *, int screen, int job, struct FileHandle *file);
	/// Request small thumbnail for a file
	int (*on_request_thumbnail)(struct Module *, int screen, int job, struct FileHandle *file);
	/// Request metadata for a file
	int (*on_request_file_metadata)(struct Module *, int screen, int job, struct FileHandle *file);
	/// Process an arbritrary command (from a console)
	int (*on_custom_command)(struct Module *, const char *request);
};

int pak_mod_add_file_thumbnail(struct Module *mod, struct FileHandle *file, void *image_data, unsigned int length);
int pak_mod_add_file_metadata(struct Module *mod, struct FileHandle *file, const struct FileMetadata *metadata);
int pak_mod_add_file_contents(struct Module *mod, struct FileHandle *file, void *image_data, unsigned int length);

/// Force the frontend to enter a screen. May not have intended effect (entering image viewer without an associating image)
int pak_mod_enter_screen(struct Module *mod, int screen);
/// Enter a custom screen
int pak_mod_enter_custom_screen(struct Module *mod);
/// Set the percent of the current job's progress bar from 0-100. Is 100 by default for each job.
int pak_mod_set_progress_bar(struct Module *mod, int job, int percent);
/// Report how many bytes are being downloaded currently in X amount of time.
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
