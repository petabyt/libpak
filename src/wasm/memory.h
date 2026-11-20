#ifndef PAK_WASM_MEMORY_H
#define PAK_WASM_MEMORY_H

#include <stdint.h>
#include <wasm_export.h>

/* The caller validates the entire range before reading or writing fields.
 * Byte-wise access supports unaligned guest data and either host byte order. */
static inline uint32_t wasm_read_u32le(const void *ptr) {
	const uint8_t *p = ptr;
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline uint64_t wasm_read_u64le(const void *ptr) {
	const uint8_t *p = ptr;
	return wasm_read_u32le(p) | ((uint64_t)wasm_read_u32le(p + 4) << 32);
}

static inline void wasm_put_u8le(uint8_t *p, uint8_t value) { p[0] = value; }
static inline void wasm_put_i8le(uint8_t *p, int8_t value) { wasm_put_u8le(p, (uint8_t)value); }
static inline void wasm_put_u16le(uint8_t *p, uint16_t value) {
	p[0] = (uint8_t)value;
	p[1] = (uint8_t)(value >> 8);
}
static inline void wasm_put_u32le(void *ptr, uint32_t value) {
	uint8_t *p = ptr;
	p[0] = (uint8_t)value;
	p[1] = (uint8_t)(value >> 8);
	p[2] = (uint8_t)(value >> 16);
	p[3] = (uint8_t)(value >> 24);
}
static inline void wasm_put_i32le(uint8_t *p, int32_t value) { wasm_put_u32le(p, (uint32_t)value); }

/* Offset zero is not rejected here: callers decide whether their API treats
 * it as a null pointer before asking WAMR to validate the memory range. */
static inline void *wasm_guest_addr(wasm_module_inst_t inst, uint32_t offset, uint32_t size) {
	if (!wasm_runtime_validate_app_addr(inst, offset, size)) return NULL;
	return wasm_runtime_addr_app_to_native(inst, offset);
}

#endif
