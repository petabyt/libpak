#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <runtime.h>
#include <runtime_ext.h>

int main(int argc, char **argv) {
	assert(argc == 2);
	FILE *f = fopen(argv[1], "rb"); assert(f);
	assert(!fseek(f, 0, SEEK_END)); long len = ftell(f); assert(len > 0); rewind(f);
	char *bytes = malloc(len); assert(bytes);
	assert(fread(bytes, 1, len, f) == (size_t)len); fclose(f);
	struct PakModule m = {0}; m.bt = pak_bt_get_context(); assert(m.bt);
	assert(!setup_wasm_module(&m, bytes, len)); free(bytes);
	assert(m.on_run_test);
	int rc = m.on_run_test(&m, 1);
	if (rc) fprintf(stderr, "Bluetooth guest returned %d\n", rc);
	assert(!rc);
	assert(m.free && !m.free(&m));
	struct PakBtAdapter *a = pak_bt_get_adapter(m.bt, 0); assert(a);
	struct PakBtDevice *d = pak_bt_get_device(m.bt, a, 0, 0); assert(d);
	assert(!pak_bt_device_disconnect(m.bt, d));
	assert(!pak_bt_unref_device(m.bt, d));
	assert(!pak_bt_unref_adapter(m.bt, a));
	pak_bt_unref_context(m.bt);
	return 0;
}
