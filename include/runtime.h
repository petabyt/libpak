// A runtime similar to Linux kernel module system that allows libraries
// that implement proprietary device protocols to be created and used
// independently of libpak
#ifndef PAKRT_H
#define PAKRT_H
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
#define PAK_DEVICE_LENS "lens"
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
#define PAK_PROP_BATTERY_MAIN "battery-main"
// For earbuds
#define PAK_PROP_BATTERY_LEFT "battery-left"
#define PAK_PROP_BATTERY_RIGHT "battery-right"
/// @}

/// @addtogroup PakCommand
/// @{
#define PAK_CMD_SHUTTER_DOWN "shutter-down"
#define PAK_CMD_SHUTTER_UP "shutter-up"
#define PAK_CMD_FOCUS_DOWN "focus-down"
#define PAK_CMD_FOCUS_UP "focus-up"
/// @}

enum PakSortedBy {
	PAK_DEFAULT = 0,
	PAK_NEWEST_FIRST = 1,
	PAK_OLDEST_FIRST = 2,
	PAK_LARGEST_FIRST = 3,
	PAK_SMALLEST_FIRST = 4,
};

struct PakFileHandle {
	/// zero based index in the current folder
	int index_in_view;
	/// name of storage device
	const char *storage_name;
	/// empty string by default. will be changed if folders are entered like 'DCIM/FUJI001'
	const char *path;
};

struct PakFileMetadata {
	const char *filename;
	// Mime types are compatible with IANA: https://www.iana.org/assignments/media-types/media-types.xhtml
	const char *mime_type;
	uint64_t file_size;
	int image_width;
	int image_height;
	// should be 0 for landscape and 270 for portrait
	int orientation;
};

/// A widget displayed on the dashboard that can show data or be manipulated by the user
struct PakWidget {
	/// Unique name (id) of the widget
	const char *name;
	/// Human readable title for the widget
	const char *title;
	/// TODO: null = dashboard, "secondary" is for "more" settings page
	const char *group;
	enum WidgetType {
		PAK_BUTTON = 0,
		PAK_BOOLEAN,
		PAK_INT,
		PAK_SLIDER,
		PAK_STRING,
		PAK_DROPDOWN,
		PAK_GRAPH,
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
			int index_value;
		}dropdownv;
		struct SettingGraph {
			const char *x_axis_name;
			const char *y_axis_name;
			int n_points;
			int *points;
		}graphv;
	}u;
};

/// Saved info about a connection that will be returned again for subsequent connections
struct PakSavedConnection {
	/// String unique to a device (such as mac address) that will be used to remember it
	/// Must be the same once reconnected
	const char *unique_id;
	/// Human readable name of device
	const char *name;
	/// Optional auxiliary data that can be used to store tokens or keys required to reconnect to a device.
	/// Set to NULL if not used.
	const uint8_t *aux_data;
	unsigned int aux_data_length;
};

enum PakTransport {
	/// Bluetooth classic and low-energy
	PAK_BLUETOOTH = 1,
	/// USB host access
	PAK_USB = 2,
	/// Connect to a WiFi access point
	PAK_WIFI_AP = 3,
	/// Expose a USB device through OTG mode
	PAK_USB_DEVICE_MODE = 4,
	/// Host an access point (hotspot) for something to connect to
	PAK_HOST_WIFI_AP = 5,
	/// Listen to/broadcast datagram packets on local network
	PAK_LOCAL_NETWORK_UDP = 6,
	/// Connect to internet service
	PAK_INTERNET = 7,
};

enum PakScreen {
	PAK_SCREEN_NONE = 0,
	/// Primary connection page for all transports
	PAK_SCREEN_CONNECT = 1,
	// Allows user to disconnect, change settings, switch to most other screens
	PAK_SCREEN_DASHBOARD = 101,
	/// A gallery of folders, files, videos, or photos. Upon selecting a file, on_switch_screen
	/// will be called with PAK_SCREEN_FILE_GALLERY->PAK_SCREEN_FILE_GALLERY
	/// Gallery may be a table of files with detailed info, or a thumbnail gallery of variable width.
	/// In what order the files iterated through (LIFO/FIFO) can be controlled.
	PAK_SCREEN_FILE_GALLERY = 102,
	/// A zoomable image viewer or video player. User may swipe left or right to view next/previous file. When this happens,
	/// on_switch_screen will be called with PAK_SCREEN_FILE_VIEWER->PAK_SCREEN_FILE_VIEWER
	PAK_SCREEN_FILE_VIEWER = 103,
	/// Can be used to send location data to the camera or apply location metadata to a specific photo.
	PAK_SCREEN_GEOTAGGING = 104,
	/// A constant live view of the camera's sensor. Allows setting various settings such as ISO, aperture, exposure settings, image settings, or video settings.
	/// Allows recording video, taking photos, controlling focus, zoom, or SLR mirror.
	PAK_SCREEN_LIVEVIEW = 105,
	/// A feed of incoming files that are being created/captured/sent by the device.
	PAK_SCREEN_LIVE_FEED = 106,
	/// Capture and control focus without liveview
	PAK_SCREEN_INTERVALOMETER = 107,
};

struct PakTimestamp {
	unsigned int year;
	unsigned int month;
	unsigned int day;
	unsigned int hour;
	unsigned int minute;
	unsigned int second;
	unsigned int centisecond;
};

struct PakLocation {
    double latitude;
    double longitude;
    double altitude;
    unsigned int satellites;
};

/// Strictly compatible with libusb1.0
typedef struct libusb_device libusb_device;

/// @brief A library that connects to and performs actions with an external device.
/// @info Most "on_" methods are given a job number. This job number can be passed to other functions
/// to check if the job is cancelled, set current progress, etc
/// @info Each method is fully blocking and thread safe by default.
struct PakModule {
	struct PakNet *net;
	struct PakBt *bt;

	/// Instance specific runtime data
	struct RuntimePriv *rt;
	/// Priv pointer that can be optionally used by module instance
	struct ModulePriv *priv;
	/// Initialize global variables or context data in priv field
	int (*init)(struct PakModule *);
	/// Free or release all memory associated with this module instance
	int (*free)(struct PakModule *);
	/// Request to try to initiate a connection over a network handle
	int (*on_try_connect_wifi)(struct PakModule *, struct PakWiFiAdapter *handle, struct PakSavedConnection *saved, int job);
	/// Request to try to initiate connection for a Bluetooth device
	/// returns zero if device is supported and connetion established 
	int (*on_try_connect_bluetooth)(struct PakModule *, struct PakBtDevice *handle, struct PakSavedConnection *saved, int job);
	/// Request to try to initiate a connection for a USB device
	int (*on_try_connect_usb)(struct PakModule *, libusb_device *dev, struct PakSavedConnection *saved, int job);
	/// Fallback method to manually find a device to connect to if on_try_* methods didn't work
	int (*on_find_connection)(struct PakModule *, int job);
	/// Runs immediately after successful connection. Runs at a constant interval, 1s by default.
	int (*on_idle_tick)(struct PakModule *, unsigned int us_since_last_tick);
	/// On user requested disconnect
	int (*on_disconnect)(struct PakModule *);
	// Runs when switching to a new screen, or switching back to an previous screen.
	int (*on_switch_screen)(struct PakModule *, int old_screen, int new_screen, int job);
	/// Request entire contents of a file
	/// send info back with pak_rt_add_file_contents
	int (*on_request_file_contents)(struct PakModule *, int job, struct PakFileHandle *file);
	/// Request small thumbnail for a file
	/// send info back with pak_rt_add_file_thumbnail
	int (*on_request_file_thumbnail)(struct PakModule *, int job, struct PakFileHandle *file);
	/// Request metadata for a file
	/// send info back with pak_rt_add_file_metadata
	int (*on_request_file_metadata)(struct PakModule *, int job, struct PakFileHandle *file);
	/// Request liveview frame
	/// send liveview frame contents with pak_rt_add_file_contents
	int (*on_request_liveview_frame)(struct PakModule *, int job, struct PakFileHandle *file);
	/// Runs when a setting has been changed by 
	int (*on_setting_changed)(struct PakModule *, int job, struct PakWidget *setting);
	/// On request to run unit test
	int (*on_run_test)(struct PakModule *, int job);
	/// Process an arbitrary command
	int (*on_custom_command)(struct PakModule *, int job, int argc, const char * const *argv);
};

struct PakStorageInfo {
	unsigned n_files_total;
	/// How the file list data set is sorted by default - includes folders
	enum PakSortedBy sorted_by;
	uint64_t size_bytes;
	uint64_t used_bytes;
	/// If this is a non-physical storage medium where files will be automatically downloaded
	/// (such as a tethered camera's temporary photo storage)
	int is_live;
};

/// Set info for a storage device by the name of storage_name
/// If storage device doesn't exist, it will be created. Otherwise it will be updated
/// A root folder will autmatically be created with n_items, call pak_rt_add_folder_info to overwrite this
int pak_rt_set_storage_info(struct PakModule *mod, const char *storage_name, struct PakStorageInfo *info);
/// If on_request_file_metadata is called on a folder, call this to specify the number of files in it instead of pak_rt_add_file_metadata
/// @param folder_path Folder names separated by '/'.
/// TODO: sorted_by inherited from storageinfo?
int pak_rt_add_folder_info(struct PakModule *mod, const char *storage_name, const char *folder_path, unsigned int n_items, enum PakSortedBy sorted_by);
/// Submit metadata for a file
/// @info May be freed and requested again later
int pak_rt_add_file_metadata(struct PakModule *mod, struct PakFileHandle *file, const struct PakFileMetadata *metadata);
/// Submit thumbnail contents for a file
/// @info May be freed and requested again later
int pak_rt_add_file_thumbnail(struct PakModule *mod, struct PakFileHandle *file, void *image_data, unsigned int length);
/// Submit contents for a file for the user to view or download
/// Can be submitted in partial, when offset + length < total_size.
/// total_size can also be zero if you don't know the length beforehand,
/// but a terminating call must be made with length = 0
int pak_rt_add_file_contents(struct PakModule *mod, struct PakFileHandle *file, void *image_data, unsigned int length, uint64_t offset, uint64_t total_size);
/// Registers a widget that is displayed in the UI and can be modified by the user
int pak_rt_set_widget(struct PakModule *mod, const struct PakWidget *s);
/// Returns true/nonzero if user requested to cancel the job (or disconnect)
int pak_rt_is_job_cancelled(struct PakModule *mod, int job);
/// Report a fatal error and prevent any more jobs from being issued
void pak_rt_fatal_error(struct PakModule *mod, const char *fmt, ...);
/// Submit error message for user to read for a non-fatal error on a job that's in progress
void pak_rt_error_message(struct PakModule *mod, int job, const char *fmt, ...);
/// Enable or disable a screen
int pak_rt_set_screen_supported(struct PakModule *mod, int screen, int v);
/// Set the percent of a job's progress bar from 0-100.
int pak_rt_set_progress_bar(struct PakModule *mod, int job, int percent);
/// Report how many bytes are being downloaded for a job currently in X amount of microseconds. TODO: currently ms
int pak_rt_set_download_stats(struct PakModule *mod, int job, long time, unsigned int n_bytes);
/// Set the unique ID of the current connected device. Will be stored for future use.
/// If the string is already stored, it will be loaded to the current session.
int pak_rt_save_session_signature(struct PakModule *mod, struct PakSavedConnection *info);
/// Report device information to the UI
int pak_rt_set_session_property(struct PakModule *mod, const char *key, const char *value);
int pak_rt_set_session_property_int(struct PakModule *mod, const char *key, int value);
/// Set the tick interval in microseconds
int pak_rt_set_tick_interval(struct PakModule *mod, unsigned int us);
/// Log debug info to user-viewable console
void pak_debug_log(struct PakModule *mod, const char *fmt, ...);
/// Log verbose info into a buffer that can be dumped for a bug report
void pak_verbose_log(struct PakModule *mod, const char *fmt, ...);
/// Get metadata from file handle
/// free with pak_rt_release_metadata
struct PakFileMetadata *pak_rt_get_metadata(struct PakModule *mod, struct PakFileHandle *file);
/// Release returned metadata handle
void pak_rt_release_metadata(struct PakModule *mod, struct PakFileMetadata *md);
/// Get option name that was selected during setup, NULL if none selected, do not free
const char *pak_rt_get_setup_option(struct PakModule *mod);
/// Return string of client name, do not free
const char *pak_rt_get_client_name(void);
/// Covers 'Bluetooth -> WiFi' handover case common in some devices
/// onTryConnectWiFi will be called
int pak_rt_add_wifi_connection(struct PakModule *mod, struct PakWiFiApFilter *filter, const char *setup_option);
/// Adds a RTSP livestream source that will be independently managed by the runtime.
/// on_request_liveview_frame won't be called 
int pak_rt_add_rtsp_livestream(struct PakModule *mod, struct PakWiFiAdapter *adapter, const char *url);

int pak_rt_set_liveview_info(struct PakModule *mod, int width, int height, int fps);

/// Get path for downloading a file
__attribute__((unused)) const char *pak_rt_get_path(struct PakModule *mod, const char *filename);
/// Notify to the runtime that the device is disconnected and to stop issuing new jobs immediately.
__attribute__((unused)) int pak_rt_disconnect(struct PakModule *mod, const char *reason);
/// Force the frontend to enter a screen. May not have intended effect (entering image viewer without an associating image)
__attribute__((unused)) int pak_rt_enter_screen(struct PakModule *mod, int screen);
#endif
