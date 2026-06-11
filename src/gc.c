#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <runtime.h>
#include <bluetooth.h>
#include <wifi.h>
#include "gc.h"

void pak_setup_gc(struct GcContext *ctx) {
	ctx->list = calloc(sizeof(struct Container), 100);
	ctx->list_length = 100;
}

void pak_gc_add_ex(struct GcContext *ctx, enum ContainerType type, void *ptr, void (*free)(void *value, void *arg), void *free_arg) {
	for (unsigned int i = 0; i < ctx->list_length; i++) {
		if (ctx->list[i].value == NULL) {
			ctx->list[i] = (struct Container){
				.type = type,
				.value = ptr,
				.free = free,
				.free_arg = free_arg,
			};
			return;
		}
		if ((i + 1) < ctx->list_length) {
			unsigned int oldlen = ctx->list_length;
			ctx->list_length += 100;
			ctx->list = realloc(ctx->list, sizeof(struct Container) * ctx->list_length);
			memset(&ctx->list[oldlen], 0, sizeof(struct Container) * (ctx->list_length - oldlen));
		}
	}
}

void pak_gc_add(struct GcContext *ctx, enum ContainerType type, void *ptr) {
	pak_gc_add_ex(ctx, type, ptr, NULL, NULL);
}

void pak_gc_remove(struct GcContext *ctx, void *ptr) {
	for (unsigned int i = 0; i < ctx->list_length; i++) {
		if (ctx->list[i].value == ptr) {
			memset(&ctx->list[i], 0, sizeof(struct Container));
			return;
		}
	}
}

void pak_gc_close(struct GcContext *ctx) {
	for (unsigned int i = 0; i < ctx->list_length; i++) {
		if (ctx->list[i].value != NULL) {
			if (ctx->list[i].free != NULL) {
				ctx->list[i].free(ctx->list[i].value, ctx->list[i].free_arg);
				pak_global_log("Container of type %d ptr %p leaked, freed by gc\n", ctx->list[i].type, ctx->list[i].value);
			} else {
				pak_global_log("Container of type %d ptr %p leaked, not freed by gc\n", ctx->list[i].type, ctx->list[i].value);
			}
		}
	}
	free(ctx->list);
}
