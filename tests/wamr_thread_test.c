/* Test-only callback worker; this is not the production Bluetooth bridge. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bluetooth.h>
#include <wasm_export.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int pending, delivered, stopping, success, started;
static uint32_t callback;
static wasm_thread_t thread;
static wasm_module_inst_t original;
static struct PakBt *bt;
static struct PakBtAdapter *adapter;
static struct PakBtDevice *device;

static int on_event(struct PakBt *ctx, enum PakBtEvent type, struct PakBtDevice *dev,
		struct PakGattCharacteristic *chr, void *arg) {
	(void)ctx; (void)dev; (void)chr; (void)arg;
	if (type != PAK_BT_EVENT_CONNECTED) return 0;
	pthread_mutex_lock(&lock);
	if (!stopping) pending = 1;
	pthread_cond_broadcast(&changed);
	pthread_mutex_unlock(&lock);
	return 0;
}

static void *worker(wasm_exec_env_t env, void *arg) {
	(void)arg;
	pthread_mutex_lock(&lock);
	while (!pending && !stopping) pthread_cond_wait(&changed, &lock);
	int run = pending && !stopping;
	pthread_mutex_unlock(&lock);
	if (run) {
		wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
		uint32_t args[] = {42};
		success = wasm_runtime_call_indirect(env, callback, 1, args);
		if (!success) fprintf(stderr, "callback: %s\n", wasm_runtime_get_exception(inst));
		printf("spawned instance pointer %s original\n", inst == original ? "equals" : "differs from");
	}
	pthread_mutex_lock(&lock);
	delivered = 1;
	pthread_cond_broadcast(&changed);
	pthread_mutex_unlock(&lock);
	return NULL;
}

static int register_callback(wasm_exec_env_t env, uint32_t index) {
	wasm_memory_inst_t memory = wasm_runtime_get_default_memory(wasm_runtime_get_module_inst(env));
	if (!memory || !wasm_memory_get_shared(memory) || started) return -1;
	callback = index;
	if (pak_bt_set_device_callback(bt, device, on_event, NULL)) return -1;
	if (wasm_runtime_spawn_thread(env, &thread, worker, NULL)) {
		return -1;
	}
	started = 1;
	return 0;
}

static void wait_for_event(wasm_exec_env_t env) {
	(void)env;
	/* The fake backend emits synchronously; the worker delivers asynchronously. */
	if (pak_bt_device_connect(bt, device, 1)) return;
	pthread_mutex_lock(&lock);
	while (!delivered) pthread_cond_wait(&changed, &lock);
	pthread_mutex_unlock(&lock);
}

int main(int argc, char **argv) {
	if (argc < 2 || argc > 3) return 2;
	int unshared = argc == 3 && !strcmp(argv[2], "unshared");
	FILE *f = fopen(argv[1], "rb");
	if (!f || fseek(f, 0, SEEK_END)) return 2;
	long length = ftell(f);
	if (length <= 0 || length > UINT32_MAX || fseek(f, 0, SEEK_SET)) return 2;
	uint8_t *bytes = malloc((size_t)length);
	if (!bytes || fread(bytes, 1, (size_t)length, f) != (size_t)length) return 2;
	fclose(f);
	bt = pak_bt_get_context();
	adapter = pak_bt_get_adapter(bt, 0);
	device = pak_bt_get_device(bt, adapter, 0, 0);
	if (!bt || !adapter || !device) return 2;
	RuntimeInitArgs init = {0};
	init.mem_alloc_type = Alloc_With_Allocator;
	init.mem_alloc_option.allocator.malloc_func = malloc;
	init.mem_alloc_option.allocator.realloc_func = realloc;
	init.mem_alloc_option.allocator.free_func = free;
	init.max_thread_num = 2;
	char error[256] = {0};
	if (!wasm_runtime_full_init(&init)) return 2;
	NativeSymbol symbols[] = {
		{"wait_for_event", (void *)wait_for_event, "()", NULL},
		{"register_callback", (void *)register_callback, "(i)i", NULL}
	};
	if (!wasm_runtime_register_natives("env", symbols, 2)) return 2;
	wasm_module_t module = wasm_runtime_load(bytes, (uint32_t)length, error, sizeof(error));
	if (!module) { fprintf(stderr, "load: %s\n", error); return 2; }
	original = wasm_runtime_instantiate(module, 16384, 16384, error, sizeof(error));
	if (!original) { fprintf(stderr, "instantiate: %s\n", error); return 2; }
	wasm_exec_env_t env = wasm_runtime_create_exec_env(original, 16384);
	wasm_function_inst_t run = wasm_runtime_lookup_function(original, "run");
	uint32_t result[1] = {0};
	if (!env || !run) return 2;
	int called = wasm_runtime_call_wasm(env, run, 0, result);
	if (!called) fprintf(stderr, "run: %s\n", wasm_runtime_get_exception(original));
	/* No further fake-backend operations occur before device destruction. */
	if (started) {
		pthread_mutex_lock(&lock);
		stopping = 1;
		pthread_cond_broadcast(&changed);
		pthread_mutex_unlock(&lock);
		if (wasm_runtime_join_thread(thread, NULL)) success = 0;
	}
	int ok = called && (unshared ? (!started && result[0] == UINT32_MAX)
	                                  : (started && success && result[0] == 42));
	printf("%s result: %d\n", unshared ? "unshared registration" : "callback shared value", (int32_t)result[0]);
	wasm_runtime_destroy_exec_env(env);
	wasm_runtime_deinstantiate(original);
	wasm_runtime_unload(module);
	wasm_runtime_destroy();
	pak_bt_unref_device(bt, device);
	pak_bt_unref_adapter(bt, adapter);
	pak_bt_unref_context(bt);
	free(bytes);
	return ok ? 0 : 1;
}
