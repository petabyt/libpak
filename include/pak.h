// Common/core pak APIs and enums
#ifndef LIBPAK_H
#define LIBPAK_H
#include <stdint.h>

enum PakErrorCode {
	PAK_ERR_NON_FATAL = -1,
	/// IO or protocol error
	PAK_ERR_IO = -2,
	/// Undefined or illegal behavior
	PAK_ERR_UNDEFINED = -3,
	/// Permission rejected
	PAK_ERR_PERMISSION = -4,
	/// Unsupported feature or API
	PAK_ERR_UNSUPPORTED = -5,
	/// Unimplemented feature
	PAK_ERR_UNIMPLEMENTED = -6,
	/// Sudden device disconnect
	PAK_ERR_DISCONNECTED = -7,
	/// Unable to establish connection
	PAK_ERR_NO_CONNECTION = -8,
	/// Operation was cancelled and was not finished
	PAK_ERR_CANCELLED = -9,
	/// Operation has not completed yet
	PAK_ERR_DEFERRED = -10,
};

enum PakBtOptions {
	PAK_BLOCK = 0,
	PAK_NONBLOCK = 1,
};

enum PakThreadPolicy {
	PAK_NO_CONCURRENT_JOBS = 0,
	PAK_CONCURRENT_JOBS = 1,
};

/// Runs main loop that supports callbacks
/// Returns nonzero on unrecoverable error
int pak_main_loop(void);
int pak_kill_main_loop(void);

void pak_error_log(const char *fmt, ...);
void pak_global_log(const char *fmt, ...);
void pak_abort(const char *fmt, ...);
#endif
