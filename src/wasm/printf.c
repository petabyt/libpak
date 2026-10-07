#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wasm_c_api.h>
#include <wasm_export.h>
#include <runtime.h>
#include "main.h"

static int emit_decimal(printf_out_fn out, void *out_ctx, uint32_t value, int *count) {
	char buf[10];
	uint32_t i = 0;

	// 32 bit decimal is at most 10 digits; 0 emits "0"
	do {
		buf[i++] = (char)('0' + value % 10);
		value /= 10;
	} while (value);

	while (i > 0) {
		if (out(out_ctx, buf[--i]))
			return -1;
		(*count)++;
	}

	return 0;
}

static int emit_hex(printf_out_fn out, void *out_ctx, uint32_t value, int *count) {
	static const char digits[] = "0123456789abcdef";
	char buf[8];
	uint32_t i = 0;

	do {
		buf[i++] = digits[value & 0xf];
		value >>= 4;
	} while (value);

	while (i > 0) {
		if (out(out_ctx, buf[--i]))
			return -1;
		(*count)++;
	}

	return 0;
}

int printf_core(printf_out_fn out, void *out_ctx, const char *fmt, void *next_int_ctx,
                 uint32_t (*next_int)(void *next_int_ctx), void *next_str_ctx,
                 const char *(*next_str)(void *next_str_ctx)) {
	int count = 0;

	if (!fmt)
		return -1;

	while (*fmt) {
		char c = *fmt++;

		if (c != '%') {
			if (out(out_ctx, c))
				return count;
			count++;
			continue;
		}

		// lone '%' at the end of the format string
		if (!*fmt) {
			if (out(out_ctx, '%'))
				return count;
			count++;
			break;
		}

		switch (*fmt++) {
			case 'd':
			{
				// INT_MIN is negated in the unsigned domain
				uint32_t u = next_int_ctx ? next_int(next_int_ctx) : 0;
				uint32_t mag = (u & 0x80000000u) ? (uint32_t) - (int32_t)u : u;

				if (u & 0x80000000u) {
					if (out(out_ctx, '-'))
						return count;
					count++;
				}

				if (emit_decimal(out, out_ctx, mag, &count))
					return count;
				break;
			}
			case 'x':
				if (emit_hex(out, out_ctx, next_int_ctx ? next_int(next_int_ctx) : 0, &count))
					return count;
				break;
			case 's':
			{
				const char *s = next_str_ctx ? next_str(next_str_ctx) : NULL;

				if (!s)
					s = "(null)";
				while (*s) {
					if (out(out_ctx, *s++))
						return count;
					count++;
				}
				break;
			}
			case '%':
				if (out(out_ctx, '%'))
					return count;
				count++;
				break;
			default:
				// unknown specifier: emit verbatim, consume nothing
				if (out(out_ctx, '%'))
					return count;
				if (out(out_ctx, fmt[-1]))
					return count;
				count += 2;
				break;
		}
	}

	return count;
}

const char *wasm_next_str(void *user) {
	struct wasm_args_ctx *ctx = (struct wasm_args_ctx *)user;
	const uint8_t *p;
	uint32_t app_addr;

	if (!wasm_runtime_validate_native_addr(ctx->inst, ctx->p, 4)) {
		ctx->inst = NULL;
		return NULL;
	}

	p = (uint8_t *)ctx->p;
	app_addr = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
	ctx->p += 4;

	if (app_addr == 0)
		return NULL;

	if (!wasm_runtime_validate_app_str_addr(ctx->inst, app_addr)) {
		ctx->inst = NULL;
		return NULL;
	}

	return (const char *)wasm_runtime_addr_app_to_native(ctx->inst, app_addr);
}

