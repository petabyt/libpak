// Common/core pak APIs and enums
#ifndef LIBPAK_H
#define LIBPAK_H
#include <stdint.h>

// Place after pointer for abi compatible packed structs
typedef char _pad_pointer[(sizeof(uintptr_t) - sizeof(uint32_t)) ? 4 : 0];

enum PakErrorCode {
	/// IO or protocol error
	PAK_ERR_IO = -1,
	/// Undefined or illegal behavior
	PAK_ERR_UNDEFINED = -2,
	/// Permission rejected
	PAK_ERR_PERMISSION = -3,
	/// Unsupported feature or API
	PAK_ERR_UNSUPPORTED = -4,
	/// Unimplemented feature
	PAK_ERR_UNIMPLEMENTED = -5,
	/// Sudden device disconnect
	PAK_ERR_DISCONNECTED = -6,
	/// Unable to establish connection
	PAK_ERR_NO_CONNECTION = -7,
};

/// Runs main loop that supports callbacks
/// Returns nonzero on unrecoverable error
/// TODO: Add way to kill this
int pak_main_loop(void);

void pak_error(const char *fmt, ...);
void pak_abort(const char *fmt, ...);
void pak_global_log(const char *fmt, ...);
#endif
