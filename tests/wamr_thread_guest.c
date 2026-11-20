#include <stdatomic.h>
#include <stdint.h>

__attribute__((import_module("env"), import_name("wait_for_event")))
void wait_for_event(void);

static _Atomic int event_value;

static void bluetooth_callback(int value) {
	atomic_store(&event_value, value);
}

__attribute__((export_name("callback_index")))
uint32_t callback_index(void) {
	return (uint32_t)(uintptr_t)bluetooth_callback;
}

__attribute__((export_name("run")))
int run(void) {
	wait_for_event();
	return atomic_load(&event_value);
}
