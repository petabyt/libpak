#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <runtime.h>
#include <runtime_ext.h>

int main(int argc, char **argv) {
	if (argc != 3) return 1;
	sigset_t set; sigemptyset(&set); sigaddset(&set, SIGUSR1);
	if (sigprocmask(SIG_BLOCK, &set, NULL)) return 2;
	char pid[32]; snprintf(pid, sizeof(pid), "%ld", (long)getpid());
	pid_t child = fork();
	if (child < 0) return 3;
	if (!child) {
		execl(argv[1], argv[1], "fuji_x_h1", "tcp", "--ip", "127.0.0.1", "--sig", pid, (char *)NULL);
		_exit(127);
	}
	int result = 1;
	struct timespec wait = {10, 0};
	if (sigtimedwait(&set, NULL, &wait) != SIGUSR1) { fprintf(stderr, "vcam did not become ready\n"); goto done; }
	FILE *f = fopen(argv[2], "rb");
	if (!f) goto done;
	if (fseek(f, 0, SEEK_END)) { fclose(f); goto done; }
	long len = ftell(f); rewind(f);
	if (len <= 0) { fclose(f); goto done; }
	char *bytes = malloc((size_t)len);
	if (!bytes) { fclose(f); goto done; }
	if (fread(bytes, 1, (size_t)len, f) != (size_t)len) { free(bytes); fclose(f); goto done; }
	fclose(f);
	struct PakModule mod = {0};
	if (setup_wasm_module(&mod, bytes, (unsigned)len)) { free(bytes); goto done; }
	free(bytes);
	int rc = mod.on_run_test ? mod.on_run_test(&mod, 0) : -1;
	if (rc) fprintf(stderr, "vcam TCP guest returned %d\n", rc);
	result = rc != 0;
	if (mod.free(&mod)) result = 1;
done:
	kill(child, SIGTERM);
	waitpid(child, NULL, 0);
	return result;
}
