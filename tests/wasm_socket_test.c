#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <runtime.h>
#include <runtime_ext.h>

int main(int argc, char **argv) {
	assert(argc == 2);
	FILE *f = fopen(argv[1], "rb"); assert(f);
	assert(!fseek(f, 0, SEEK_END)); long len = ftell(f); assert(len > 0); rewind(f);
	char *bytes = malloc((size_t)len); assert(bytes);
	assert(fread(bytes, 1, (size_t)len, f) == (size_t)len); fclose(f);
	struct PakModule mod = {0};
	assert(!setup_wasm_module(&mod, bytes, (unsigned)len));
	free(bytes);
	assert(mod.on_run_test);
	int rc = mod.on_run_test(&mod, 0);
	if (rc) fprintf(stderr, "socket guest returned %d\n", rc);
	assert(!rc);
	assert(!mod.free(&mod));
	return 0;
}
