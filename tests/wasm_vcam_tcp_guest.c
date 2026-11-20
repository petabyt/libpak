#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <runtime.h>

static int transfer(int fd, unsigned char *buf, int n, int send_data) {
	int off = 0;
	while (off < n) {
		int rc = send_data ? (int)write(fd, buf + off, n - off) : (int)read(fd, buf + off, n - off);
		if (rc <= 0) return -1;
		off += rc;
	}
	return 0;
}
static uint32_t u32(const unsigned char *p) {
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int test(struct PakModule *mod, int job) {
	(void)mod; (void)job;
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) return 1;
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK)) return 2;
	struct sockaddr_in address = {0};
	address.sin_family = AF_INET; address.sin_port = htons(55740);
	address.sin_addr.s_addr = htonl(0x7f000001);
	if (connect(fd, (struct sockaddr *)&address, sizeof(address)) && errno != EINPROGRESS) return 3;
	fd_set writes; FD_ZERO(&writes); FD_SET(fd, &writes);
	struct timeval timeout = {5, 0};
	if (select(fd + 1, NULL, &writes, NULL, &timeout) != 1) return 4;
	int error = -1; socklen_t len = sizeof(error);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) || error) return 5;
	if (fcntl(fd, F_SETFL, flags)) return 6;
	timeout.tv_sec = 5; timeout.tv_usec = 0;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) return 7;
	/* Fuji PTP/IP InitCommandRequest, same 82-byte format as libfuji. */
	unsigned char request[82] = {82, 0, 0, 0, 1};
	request[28] = 'l'; request[30] = 'i'; request[32] = 'b'; request[34] = 'p';
	if (transfer(fd, request, sizeof(request), 1)) return 8;
	unsigned char reply[512];
	if (transfer(fd, reply, 4, 0)) return 9;
	uint32_t size = u32(reply);
	if (size < 8 || size > sizeof(reply)) return 10;
	if (transfer(fd, reply + 4, (int)size - 4, 0)) return 11;
	if (u32(reply + 4) != 2) return 12;
	if (close(fd)) return 13;
	return 0;
}
int get_module(struct PakModule *mod) { mod->on_run_test = test; return 0; }
