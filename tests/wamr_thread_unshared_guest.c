#include <stdint.h>

__attribute__((import_module("env"), import_name("register_callback")))
int register_callback(uint32_t callback);

static void bluetooth_callback(int value) { (void)value; }

__attribute__((export_name("run")))
int run(void) {
	return register_callback((uint32_t)(uintptr_t)bluetooth_callback);
}
