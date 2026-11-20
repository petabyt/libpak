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
	struct PakModule external = {0}, owned = {0};
	external.net = pak_net_get_context(); assert(external.net);
	assert(!setup_wasm_module(&external, bytes, (unsigned)len));
	assert(!setup_wasm_module(&owned, bytes, (unsigned)len));
	free(bytes);
	assert(external.on_run_test && owned.on_run_test);
	int rc = external.on_run_test(&external, 1);
	if (rc) fprintf(stderr, "external Wi-Fi guest returned %d\n", rc);
	assert(!rc);
	rc = owned.on_run_test(&owned, 1);
	if (rc) fprintf(stderr, "owned Wi-Fi guest returned %d\n", rc);
	assert(!rc);
	assert(!external.free(&external));
	struct PakWiFiAdapter *a = pak_wifi_get_adapter(external.net, 0); assert(a);
	struct PakWiFiAp *ap = pak_wifi_get_connected_ap(external.net, a); assert(ap);
	assert(!pak_wifi_unref_ap(external.net, a, ap));
	assert(!pak_wifi_unref_adapter(external.net, a));
	pak_net_unref_context(external.net);
	assert(!owned.free(&owned));
	return 0;
}
