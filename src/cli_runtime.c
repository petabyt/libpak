#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>
#include "wifi.h"
#include "bluetooth.h"
#include "runtime.h"

struct RuntimePriv {
	int current_job;
};

int pak_mod_set_progress_bar(struct Module *mod, int job, int percent) {	
	printf("%d%%\n", percent);
	return 0;
}

int pak_mod_set_current_download_speed(struct Module *mod, long time, unsigned int n_bytes) {
	return 0;
}

int test_module(struct Module *mod) {
	mod->rt = malloc(sizeof(struct RuntimePriv));
	mod->rt->current_job = 1;
	mod->init(mod);

	return 0;
}
