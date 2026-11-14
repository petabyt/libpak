#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <quickjs.h>
#include <quickjs-libc.h>
#pragma GCC diagnostic pop
#include <runtime.h>
#include <pthread.h>

struct ModulePriv {
	JSContext *ctx;
	JSRuntime *rt;
	JSValue object;
	pthread_mutex_t context_mutex;
};