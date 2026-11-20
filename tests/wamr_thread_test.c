/* Standalone WAMR concurrency probe, not the production Bluetooth bridge. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wasm_export.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int waiting, delivered, success;
static uint32_t callback;
static wasm_module_inst_t original;

static void wait_for_event(wasm_exec_env_t env) {
	(void)env;
	pthread_mutex_lock(&lock);
	waiting = 1;
	pthread_cond_broadcast(&changed);
	while (!delivered)
		pthread_cond_wait(&changed, &lock);
	pthread_mutex_unlock(&lock);
}

static void *worker(wasm_exec_env_t env, void *arg) {
	(void)arg;
	pthread_mutex_lock(&lock);
	while (!waiting)
		pthread_cond_wait(&changed, &lock);
	pthread_mutex_unlock(&lock);
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
	uint32_t args[] = {42};
	success = wasm_runtime_call_indirect(env, callback, 1, args);
	if (!success)
		fprintf(stderr, "callback: %s\n", wasm_runtime_get_exception(inst));
	printf("spawned instance pointer %s original\n", inst == original ? "equals" : "differs from");
	pthread_mutex_lock(&lock);
	delivered = 1;
	pthread_cond_broadcast(&changed);
	pthread_mutex_unlock(&lock);
	return NULL;
}

int main(int argc, char **argv) {
	if (argc != 2) return 2;
	FILE *f = fopen(argv[1], "rb");
	if (!f || fseek(f, 0, SEEK_END)) return 2;
	long length = ftell(f);
	if (length <= 0 || length > UINT32_MAX || fseek(f, 0, SEEK_SET)) return 2;
	uint8_t *bytes = malloc((size_t)length);
	if (!bytes || fread(bytes, 1, (size_t)length, f) != (size_t)length) return 2;
	fclose(f);
	RuntimeInitArgs init = {0};
	init.mem_alloc_type = Alloc_With_Allocator;
	init.mem_alloc_option.allocator.malloc_func = malloc;
	init.mem_alloc_option.allocator.realloc_func = realloc;
	init.mem_alloc_option.allocator.free_func = free;
	init.max_thread_num = 2;
	char error[256] = {0};
	if (!wasm_runtime_full_init(&init)) return 2;
	NativeSymbol symbols[] = {{"wait_for_event", (void *)wait_for_event, "()", NULL}};
	if (!wasm_runtime_register_natives("env", symbols, 1)) return 2;
	wasm_module_t module = wasm_runtime_load(bytes, (uint32_t)length, error, sizeof(error));
	if (!module) { fprintf(stderr, "load: %s\n", error); return 2; }
	original = wasm_runtime_instantiate(module, 16384, 16384, error, sizeof(error));
	if (!original) { fprintf(stderr, "instantiate: %s\n", error); return 2; }
	wasm_exec_env_t env = wasm_runtime_create_exec_env(original, 16384);
	wasm_function_inst_t index = wasm_runtime_lookup_function(original, "callback_index");
	wasm_function_inst_t run = wasm_runtime_lookup_function(original, "run");
	uint32_t result[1] = {0};
	if (!env || !index || !run || !wasm_runtime_call_wasm(env, index, 0, result)) return 2;
	callback = result[0];
	wasm_thread_t thread;
	if (wasm_runtime_spawn_thread(env, &thread, worker, NULL)) return 2;
	int called = wasm_runtime_call_wasm(env, run, 0, result);
	if (!called) fprintf(stderr, "run: %s\n", wasm_runtime_get_exception(original));
	int joined = wasm_runtime_join_thread(thread, NULL);
	int ok = called && !joined && success && result[0] == 42;
	printf("concurrent callback shared value: %u\n", result[0]);
	wasm_runtime_destroy_exec_env(env);
	wasm_runtime_deinstantiate(original);
	wasm_runtime_unload(module);
	wasm_runtime_destroy();
	free(bytes);
	return ok ? 0 : 1;
}
