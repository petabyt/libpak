// Common/core pak APIs and enums
#pragma once
#include <stdint.h>

// global ctx?
//void pak_set_error_printer(struct PakWiFi *ctx, void (*printf)(void *arg, const char *fmt, ...), void *arg);
//static void pak_error(struct Pak)

typedef char _pad_pointer[(sizeof(uintptr_t) - sizeof(uint32_t)) ? 4 : 0];

enum PakErrorCode {
	PAK_PERMISSION_DENIED = -1,
	PAK_UNSUPPORTED = -2,
};
