// Common/core pak APIs and enums
#pragma once
#include <stdint.h>

// Place after pointer for abi compatible structs
typedef char _pad_pointer[(sizeof(uintptr_t) - sizeof(uint32_t)) ? 4 : 0];

enum PakErrorCode {
	PAK_PERMISSION_DENIED = -1,
	PAK_UNSUPPORTED = -2,
	PAK_UNIMPLEMENTED = -3,
	PAK_NOT_CONNECTED = -4,
	/// Action is running on another thread and response is not ready yet
	PAK_DEFERRED = -5,
};

int pak_main_loop(void);

// Can be defined by application
//struct PakLogCtx;

void pak_error(const char *fmt, ...);
void pak_abort(const char *fmt, ...);

void pak_global_log(const char *fmt, ...);
