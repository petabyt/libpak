// Runtime
#pragma once
#include <stdint.h>
#include <wifi.h>
#include <bluetooth.h>

/// @addtogroup PakDevice
/// @{
// Photo class
#define PAK_DEVICE_PROFESSIONAL_CAMERA "professional-camera"
#define PAK_DEVICE_ACTION_CAMERA "action-camera"
#define PAK_DEVICE_DASHCAM "dashcam"
#define PAK_DEVICE_GENERIC_CAMERA "generic-camera"
#define PAK_DEVICE_WIFI_SD_CARD "wifi-sd-card"
#define PAK_DEVICE_DOORBELL "doorbell"
// Home class
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
#define PAK_DEVICE_SMART_TV "smart-tv"
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
#define PAK_DEVICE_GENERIC_AUTOMOTIVE "generic-automotive"
/// @}

/// @addtogroup PakProperty
/// @{
#define PAK_PROP_NAME "name"
#define PAK_PROP_FW_VER "firmware-version"
/// @}

struct FileHandle {
	int index_in_view;
	const char *filename;
};

struct FileMetadata {
	const char *filename;
	const char *mime_type;
};

struct PakUserSetting {
	const char *name;
	enum SettingType {
		PAK_BOOLEAN,
		PAK_INT,
		PAK_SLIDER,
		PAK_STRING,
		PAK_DROPDOWN,
	}type;
	union SettingUnion {
		struct SettingBoolean {
			int v;
		}boolv;
		struct SettingInt {
			int v;
		}intv;
		struct SettingSlider {
			int v;
			int min;
			int max;
		}slider;
		struct SettingString {
			const char *value;
		}stringv;
		struct SettingDropDown {
			const char **list;
		}dropdownv;
	}u;
};

enum PakTransport {
	/// Bluetooth low energy
	PAK_BLE = 1,
	/// Bluetooth classic
	PAK_BTC = 2,
	/// USB host access
	PAK_USB = 3,
	/// Become a USB device
	PAK_USB_DEVICE_MODE = 4,
	/// Connect to a WiFi access point
	PAK_WIFI_AP = 5,
	/// Host an access point (hotspot) for something to connect to
	PAK_HOST_WIFI_AP = 6,
	/// Listen to local network over UPnP
	PAK_LOCAL_NETWORK_UPNP_LISTEN = 7,
	/// Broadcast datagram over local network
	PAK_LOCAL_NETWORK_UPNP_BROADCAST = 8,
	/// Connect to something over the internet
	PAK_INTERNET = 9,
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

	/// Prints lines of text into a terminal-like widget
	SCREEN_CONSOLE = 100,
	// Allows user to disconnect, change settings, switch to other screens
	SCREEN_DASHBOARD,
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

/// @brief A library that connects to and performs actions with an external device.
/// @info Most "on_" methods are given a job number. This job number can be passed to other functions
/// to check if the job is cancelled, set current progress, etc
/// @info Each method is fully blocking and thread safe by default.
struct Module {
	struct PakNet *net;
	struct PakBt *bt;

	// Instance specific runtime data
	struct RuntimePriv *rt;
	/// Priv pointer that can be optionally used by module instance
	struct ModulePriv *priv;
	/// Initialize global variables or context data in priv field
	int (*init)(struct Module *);
	/// Free or release all memory associated with this module instance
	int (*free)(struct Module *);
	/// Use WiFi or Bluetooth APIs manually to find a device to connect to.
	int (*on_find_connection)(struct Module *, int job);
	/// Try to initiate a connection over a network handle
	int (*on_try_connect_wifi)(struct Module *, struct PakWiFiAdapter *handle, int job);
	/// Runs immediately after successful connection. Runs at a constant interval, 1s by default.
	int (*on_idle_tick)(struct Module *, unsigned int us_since_last_tick);
	/// On user requested disconnect
	int (*on_disconnect)(struct Module *);
	// Runs when switching to a new screen, or switching back to an old screen.
	int (*on_switch_screen)(struct Module *, int old_screen, int new_screen, int job);
	/// Request entire contents of a file
	/// send info back with pak_rt_add_file_contents
	int (*on_request_file_contents)(struct Module *, int screen, int job, struct FileHandle *file);
	/// Request small thumbnail for a file
	/// send info back with pak_rt_add_file_thumbnail
	int (*on_request_thumbnail)(struct Module *, int screen, int job, struct FileHandle *file);
	/// Request metadata for a file
	/// send info back with pak_rt_add_file_metadata
	int (*on_request_file_metadata)(struct Module *, int screen, int job, struct FileHandle *file);
	/// Request liveview frame
	/// send liveview frame contents with pak_rt_add_file_contents
	int (*on_request_liveview_frame)(struct Module *, int screen, int job, struct FileHandle *file);
	/// Runs when a setting has been changed by 
	int (*on_setting_changed)(struct Module *, int screen, int job, struct PakUserSetting *pane);
	/// On request to run self test, test suite, debug dumps, or other diagnostics
	int (*on_run_test)(struct Module *, int screen, int job);
	/// Process an arbritrary command
	int (*on_custom_command)(struct Module *, int argc, char **argv);
};

int pak_rt_add_file_thumbnail(struct Module *mod, struct FileHandle *file, void *image_data, unsigned int length);
int pak_rt_add_file_metadata(struct Module *mod, struct FileHandle *file, const struct FileMetadata *metadata);
int pak_rt_add_file_contents(struct Module *mod, struct FileHandle *file, void *image_data, unsigned int length);
int pak_rt_add_user_setting(struct Module *mod, const struct PakUserSetting *s);

/// Returns true if user requested to cancel the job.
int pak_rt_is_job_cancelled(struct Module *mod, int job);
/// Enable or disable a screen
int pak_rt_set_screen_supported(struct Module *mod, int screen, int v);
/// Force the frontend to enter a screen. May not have intended effect (entering image viewer without an associating image)
int pak_rt_enter_screen(struct Module *mod, int screen);
/// Enter a custom screen (TODO)
int pak_rt_enter_custom_screen(struct Module *mod);
/// Set the percent of a job's progress bar from 0-100. Is 100 by default for each job.
int pak_rt_se_progress_bar(struct Module *mod, int job, int percent);
/// Report how many bytes are being downloaded for a job currently in X amount of microseconds.
int pak_rt_set_download_stats(struct Module *mod, int job, long time, unsigned int n_bytes);
/// Set the unique ID of the current connected device. Will be stored for future use.
/// If the string is already stored, it will be loaded to the current session.
int pak_rt_set_device_unique_id(struct Module *mod, const char *string);
/// Send the name of the connected device to the runtime. Will appear in the UI and will associate
/// with the current unique ID.
int pak_rt_report_device_name(struct Module *mod, const char *name);
/// Set the battery percentage of the connected device
int pak_rt_report_device_battery(struct Module *mod, int percent);
/// Report device information to the UI
int pak_rt_set_session_property(struct Module *mod, const char *key, const char *value);
/// Notify to the runtime that the device is disconnected and to stop issuing new jobs immediately.
int pak_rt_disconnect(struct Module *mod, const char *reason);
/// Set the tick interval in microseconds
int pak_rt_set_tick_interval(struct Module *mod, unsigned int us);
/// Get path for downloading a file
//const char *pak_rt_get_path(struct Module *mod, const char *filename);

struct Module *pak_create_mod(void);
int pak_rt_test_module(struct Module *mod);
struct Module *pak_rt_mod_from_native(int (*get)(struct Module *mod));
