/* POSIX entry points missing from wasi-libc, backed by WAMR's WASI sockets. */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wasm_export.h>
#include "main.h"
#include "memory.h"

// Copied from core/shared/platform/include/platform_wasi_types.h
enum {
	WASI_AF_INET = 1,
	WASI_SOCK_DGRAM = 5,
	WASI_SOCK_STREAM = 6,
	WASI_SOCKET_DGRAM = 0,
	WASI_SOCKET_STREAM = 1,
	WASI_ADDR_IPV4 = 0,
	WASI_SOL_SOCKET = 0x7fffffff,
	WASI_SOL_TCP = 6,
	WASI_SO_REUSEADDR = 2,
	WASI_SO_ERROR = 4,
	WASI_SO_BROADCAST = 6,
	WASI_SO_RCVTIMEO = 20,
	WASI_TCP_NODELAY = 1,
	WASI_EAFNOSUPPORT = 5,
	WASI_EBADF = 8,
	WASI_EINVAL = 28,
	WASI_ENOMEM = 48,
	WASI_ENOTSUP = 58,
};

extern uint32_t get_libc_wasi_export_apis(NativeSymbol **symbols);
extern int wasm_runtime_dup_wasi_socket(wasm_module_inst_t inst, uint32_t fd);
extern uint16_t convert_errno(int error);

static uint32_t (*wasi_open)(wasm_exec_env_t, uint32_t, uint32_t, uint32_t, uint32_t *);
static uint32_t (*wasi_bind)(wasm_exec_env_t, uint32_t, void *);
static uint32_t (*wasi_connect)(wasm_exec_env_t, uint32_t, void *);
static uint32_t (*wasi_listen)(wasm_exec_env_t, uint32_t, uint32_t);
static uint32_t (*wasi_local)(wasm_exec_env_t, uint32_t, void *);
static uint32_t (*wasi_remote)(wasm_exec_env_t, uint32_t, void *);
static uint32_t (*wasi_accept)(wasm_exec_env_t, uint32_t, uint32_t, void *);
static uint32_t (*wasi_close)(wasm_exec_env_t, uint32_t);
static uint32_t (*wasi_sendto)(wasm_exec_env_t, uint32_t, void *, uint32_t, uint32_t, void *, void *);
static uint32_t (*wasi_recvfrom)(wasm_exec_env_t, uint32_t, void *, uint32_t, uint32_t, void *, void *);
static uint32_t (*wasi_send)(wasm_exec_env_t, uint32_t, void *, uint32_t, uint32_t, void *);
static uint32_t (*wasi_set_reuse)(wasm_exec_env_t, uint32_t, uint32_t);
static uint32_t (*wasi_set_broadcast)(wasm_exec_env_t, uint32_t, uint32_t);
static uint32_t (*wasi_set_nodelay)(wasm_exec_env_t, uint32_t, uint32_t);
static uint32_t (*wasi_set_recv_timeout)(wasm_exec_env_t, uint32_t, uint64_t);

/* wasi-libc's errno values are WASI errno values, not the host's errno. */
static int fail(wasm_exec_env_t env, uint32_t error) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
	wasm_function_inst_t fn = wasm_runtime_lookup_function(inst, "__errno_location");
	uint32_t args[1] = {0};
	if (!fn || !wasm_runtime_call_wasm(env, fn, 0, args)) {
		if (!wasm_runtime_get_exception(inst))
			wasm_runtime_set_exception(inst, "socket: missing __errno_location export");
		return -1;
	}
	void *p = wasm_guest_addr(inst, args[0], 4);
	if (p)
		wasm_put_u32le(p, error);
	return -1;
}

/* WAMR validates pointers passed to its exports as guest linear memory. */
static int open_socket(wasm_exec_env_t env, int domain, int type, int protocol) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
	/* WASI SDK Preview 1 constants, not host AF_INET/SOCK_* values. */
	if (domain != WASI_AF_INET)
		return fail(env, WASI_EAFNOSUPPORT);
	if ((type != WASI_SOCK_DGRAM && type != WASI_SOCK_STREAM) || protocol != 0)
		return fail(env, WASI_ENOTSUP);
	void *p = NULL;
	uint32_t off = wasm_runtime_module_malloc(inst, 4, &p);
	if (!off)
		return fail(env, WASI_ENOMEM);
	uint32_t error =
		wasi_open(env, UINT32_MAX, WASI_ADDR_IPV4, type == WASI_SOCK_DGRAM ? WASI_SOCKET_DGRAM : WASI_SOCKET_STREAM, p);
	int fd = error ? -1 : (int)wasm_read_u32le(p);
	wasm_runtime_module_free(inst, off);
	return error ? fail(env, error) : fd;
}

static int address_call(wasm_exec_env_t env, int fd, uint32_t addr, uint32_t len, bool connect_call) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
	if (!addr || len < 16)
		return fail(env, WASI_EINVAL);
	const uint8_t *in = wasm_guest_addr(inst, addr, 16);
	if (!in)
		return -1;
	if (in[0] != WASI_AF_INET || in[1] != 0)
		return fail(env, WASI_EAFNOSUPPORT);
	uint8_t ip[4];
	memcpy(ip, in + 4, sizeof(ip));
	uint16_t port = (uint16_t)((uint16_t)in[2] << 8 | in[3]);
	void *p = NULL;
	uint32_t off = wasm_runtime_module_malloc(inst, 24, &p);
	if (!off)
		return fail(env, WASI_ENOMEM);
	uint8_t *out = p;
	memset(out, 0, 24);
	/* __wasi_addr_t: IPv4 tag, four network-order octets, host-order port. */
	memcpy(out + 4, ip, sizeof(ip));
	wasm_put_u16le(out + 8, port);
	uint32_t error = connect_call ? wasi_connect(env, fd, p) : wasi_bind(env, fd, p);
	wasm_runtime_module_free(inst, off);
	return error ? fail(env, error) : 0;
}

static int bind_socket(wasm_exec_env_t e, int fd, uint32_t a, uint32_t n) {
	return address_call(e, fd, a, n, false);
}

static int connect_socket(wasm_exec_env_t e, int fd, uint32_t a, uint32_t n) {
	return address_call(e, fd, a, n, true);
}

static int listen_socket(wasm_exec_env_t env, int fd, int backlog) {
	if (backlog < 0)
		backlog = 0;
	uint32_t error = wasi_listen(env, fd, (uint32_t)backlog);
	return error ? fail(env, error) : 0;
}

/* Encode/decode only the wasm32 WASI Preview 1 IPv4 sockaddr layout. */
static int encode_address(wasm_module_inst_t inst, uint32_t address, uint32_t len, uint8_t out[24]) {
	if (!address || len < 16)
		return WASI_EINVAL;
	const uint8_t *p = wasm_guest_addr(inst, address, 16);
	if (!p)
		return -1;
	if (p[0] != WASI_AF_INET || p[1])
		return WASI_EAFNOSUPPORT;
	memset(out, 0, 24);
	memcpy(out + 4, p + 4, 4);
	wasm_put_u16le(out + 8, (uint16_t)((uint16_t)p[2] << 8 | p[3]));
	return 0;
}

static int decode_address(wasm_module_inst_t inst, uint32_t address, uint32_t lenptr, const uint8_t *in) {
	if (!address || !lenptr)
		return WASI_EINVAL;
	uint8_t *lp = wasm_guest_addr(inst, lenptr, 4);
	if (!lp)
		return -1;
	uint32_t n = wasm_read_u32le(lp);
	if (n > 16)
		n = 16;
	uint8_t *out = wasm_guest_addr(inst, address, n);
	if (!out)
		return -1;
	if (wasm_read_u32le(in) != WASI_ADDR_IPV4)
		return WASI_EAFNOSUPPORT;
	uint8_t result[16] = {WASI_AF_INET, 0, in[9], in[8], in[4], in[5], in[6], in[7]};
	memcpy(out, result, n);
	wasm_put_u32le(lp, 16);
	return 0;
}

static int get_address(wasm_exec_env_t env, int fd, uint32_t address, uint32_t lenptr, bool peer) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
	if (!address || !lenptr)
		return fail(env, WASI_EINVAL);
	void *p = NULL;
	uint32_t off = wasm_runtime_module_malloc(inst, 24, &p);
	if (!off)
		return fail(env, WASI_ENOMEM);
	uint32_t error = peer ? wasi_remote(env, fd, p) : wasi_local(env, fd, p);
	uint8_t bytes[24];
	if (!error)
		memcpy(bytes, p, sizeof(bytes));
	wasm_runtime_module_free(inst, off);
	if (error)
		return fail(env, error);
	int rc = decode_address(inst, address, lenptr, bytes);
	return rc > 0 ? fail(env, rc) : rc;
}

static int get_local(wasm_exec_env_t e, int fd, uint32_t a, uint32_t n) {
	return get_address(e, fd, a, n, false);
}

static int get_peer(wasm_exec_env_t e, int fd, uint32_t a, uint32_t n) {
	return get_address(e, fd, a, n, true);
}

static int accept_socket(wasm_exec_env_t env, int fd, uint32_t addr, uint32_t lenptr) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
	if (addr && !lenptr)
		return fail(env, WASI_EINVAL);
	void *p = NULL;
	uint32_t off = wasm_runtime_module_malloc(inst, 4, &p);
	if (!off)
		return fail(env, WASI_ENOMEM);
	uint32_t error = wasi_accept(env, fd, 0, p);
	int newfd = error ? -1 : (int)wasm_read_u32le(p);
	wasm_runtime_module_free(inst, off);
	if (error)
		return fail(env, error);
	if (addr && get_peer(env, newfd, addr, lenptr)) {
		wasi_close(env, newfd);
		return -1;
	}
	return newfd;
}

static int datagram(wasm_exec_env_t env, int fd, uint32_t buf, uint32_t len, int flags, uint32_t addr, uint32_t addrlen,
					bool sending) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
	if (flags)
		return fail(env, WASI_ENOTSUP);
	if (len > INT32_MAX)
		return fail(env, WASI_EINVAL);
	if (len && !wasm_guest_addr(inst, buf, len))
		return -1;
	uint8_t encoded[24];
	if (sending && addr) {
		int rc = encode_address(inst, addr, addrlen, encoded);
		if (rc)
			return rc > 0 ? fail(env, rc) : -1;
	}
	if (!sending && addr && !addrlen)
		return fail(env, WASI_EINVAL);
	if (!sending && addr) {
		const uint8_t *lp = wasm_guest_addr(inst, addrlen, 4);
		if (!lp)
			return -1;
		uint32_t n = wasm_read_u32le(lp);
		if (n > 16)
			n = 16;
		if (!wasm_guest_addr(inst, addr, n))
			return -1;
	}
	void *scratch = NULL;
	uint32_t off = wasm_runtime_module_malloc(inst, 36, &scratch);
	if (!off)
		return fail(env, WASI_ENOMEM);
	uint8_t *p = scratch;
	memset(p, 0, 36);
	wasm_put_u32le(p, buf);
	wasm_put_u32le(p + 4, len);
	if (sending && addr)
		memcpy(p + 8, encoded, 24);
	uint32_t error;
	if (sending && !addr)
		error = wasi_send(env, fd, p, 1, 0, p + 32);
	else if (sending)
		error = wasi_sendto(env, fd, p, 1, 0, p + 8, p + 32);
	else
		error = wasi_recvfrom(env, fd, p, 1, 0, addr ? p + 8 : NULL, p + 32);
	uint32_t count = error ? 0 : wasm_read_u32le(p + 32);
	if (!error && !sending && addr)
		memcpy(encoded, p + 8, 24);
	wasm_runtime_module_free(inst, off);
	if (error)
		return fail(env, error);
	if (!sending && addr) {
		int rc = decode_address(inst, addr, addrlen, encoded);
		if (rc)
			return rc > 0 ? fail(env, rc) : -1;
	}
	return (int)count;
}

static int send_to(wasm_exec_env_t e, int fd, uint32_t b, uint32_t n, int f, uint32_t a, uint32_t al) {
	return datagram(e, fd, b, n, f, a, al, true);
}

static int recv_from(wasm_exec_env_t e, int fd, uint32_t b, uint32_t n, int f, uint32_t a, uint32_t al) {
	return datagram(e, fd, b, n, f, a, al, false);
}

static int set_option(wasm_exec_env_t env, int fd, int level, int option, uint32_t val, uint32_t len) {
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
	if (!val)
		return fail(env, WASI_EINVAL);
	if (level == WASI_SOL_SOCKET && option == WASI_SO_RCVTIMEO) {
		if (len < 16)
			return fail(env, WASI_EINVAL);
		const uint8_t *p = wasm_guest_addr(inst, val, 16);
		if (!p)
			return -1;
		uint64_t sec = wasm_read_u64le(p), usec = wasm_read_u64le(p + 8);
		if (sec > (UINT64_MAX - usec) / 1000000 || usec >= 1000000)
			return fail(env, WASI_EINVAL);
		uint32_t error = wasi_set_recv_timeout(env, fd, sec * 1000000 + usec);
		return error ? fail(env, error) : 0;
	}
	if (len < 4)
		return fail(env, WASI_EINVAL);
	const uint8_t *p = wasm_guest_addr(inst, val, 4);
	if (!p)
		return -1;
	uint32_t yes = wasm_read_u32le(p) != 0, error;
	if (level == WASI_SOL_SOCKET && option == WASI_SO_REUSEADDR)
		error = wasi_set_reuse(env, fd, yes);
	else if (level == WASI_SOL_SOCKET && option == WASI_SO_BROADCAST)
		error = wasi_set_broadcast(env, fd, yes);
	else if (level == WASI_SOL_TCP && option == WASI_TCP_NODELAY)
		error = wasi_set_nodelay(env, fd, yes);
	else
		return fail(env, WASI_ENOTSUP);
	return error ? fail(env, error) : 0;
}

static int get_option(wasm_exec_env_t env, int fd, int level, int option, uint32_t val, uint32_t lenptr) {
	if (level != WASI_SOL_SOCKET || option != WASI_SO_ERROR)
		return fail(env, WASI_ENOTSUP);
	wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
	if (!val || !lenptr)
		return fail(env, WASI_EINVAL);
	uint8_t *lp = wasm_guest_addr(inst, lenptr, 4);
	if (!lp)
		return -1;
	uint32_t n = wasm_read_u32le(lp);
	if (n > 4)
		n = 4;
	uint8_t *out = wasm_guest_addr(inst, val, n);
	if (!out)
		return -1;
	if (fd < 0)
		return fail(env, WASI_EBADF);
	int native_fd = wasm_runtime_dup_wasi_socket(inst, (uint32_t)fd);
	if (native_fd < 0)
		return fail(env, convert_errno(errno));
	int value = 0;
	socklen_t size = sizeof(value);
	int rc = getsockopt(native_fd, SOL_SOCKET, SO_ERROR, &value, &size);
	int saved_errno = errno;
	close(native_fd);
	if (rc)
		return fail(env, convert_errno(saved_errno));
	uint8_t result[4];
	wasm_put_u32le(result, value ? convert_errno(value) : 0);
	memcpy(out, result, n);
	wasm_put_u32le(lp, n);
	return 0;
}

bool wasm_socket_register(void) {
	NativeSymbol *wasi = NULL;
	uint32_t n = get_libc_wasi_export_apis(&wasi);
	for (uint32_t i = 0; i < n; i++) {
		if (!strcmp(wasi[i].symbol, "sock_open"))
			memcpy(&wasi_open, &wasi[i].func_ptr, sizeof(wasi_open));
		else if (!strcmp(wasi[i].symbol, "sock_bind"))
			memcpy(&wasi_bind, &wasi[i].func_ptr, sizeof(wasi_bind));
		else if (!strcmp(wasi[i].symbol, "sock_connect"))
			memcpy(&wasi_connect, &wasi[i].func_ptr, sizeof(wasi_connect));
		else if (!strcmp(wasi[i].symbol, "sock_listen"))
			memcpy(&wasi_listen, &wasi[i].func_ptr, sizeof(wasi_listen));
		else if (!strcmp(wasi[i].symbol, "sock_addr_local"))
			memcpy(&wasi_local, &wasi[i].func_ptr, sizeof(wasi_local));
		else if (!strcmp(wasi[i].symbol, "sock_addr_remote"))
			memcpy(&wasi_remote, &wasi[i].func_ptr, sizeof(wasi_remote));
		else if (!strcmp(wasi[i].symbol, "sock_accept"))
			memcpy(&wasi_accept, &wasi[i].func_ptr, sizeof(wasi_accept));
		else if (!strcmp(wasi[i].symbol, "fd_close"))
			memcpy(&wasi_close, &wasi[i].func_ptr, sizeof(wasi_close));
		else if (!strcmp(wasi[i].symbol, "sock_send_to"))
			memcpy(&wasi_sendto, &wasi[i].func_ptr, sizeof(wasi_sendto));
		else if (!strcmp(wasi[i].symbol, "sock_recv_from"))
			memcpy(&wasi_recvfrom, &wasi[i].func_ptr, sizeof(wasi_recvfrom));
		else if (!strcmp(wasi[i].symbol, "sock_send"))
			memcpy(&wasi_send, &wasi[i].func_ptr, sizeof(wasi_send));
		else if (!strcmp(wasi[i].symbol, "sock_set_reuse_addr"))
			memcpy(&wasi_set_reuse, &wasi[i].func_ptr, sizeof(wasi_set_reuse));
		else if (!strcmp(wasi[i].symbol, "sock_set_broadcast"))
			memcpy(&wasi_set_broadcast, &wasi[i].func_ptr, sizeof(wasi_set_broadcast));
		else if (!strcmp(wasi[i].symbol, "sock_set_tcp_no_delay"))
			memcpy(&wasi_set_nodelay, &wasi[i].func_ptr, sizeof(wasi_set_nodelay));
		else if (!strcmp(wasi[i].symbol, "sock_set_recv_timeout"))
			memcpy(&wasi_set_recv_timeout, &wasi[i].func_ptr, sizeof(wasi_set_recv_timeout));
	}
	if (!wasi_open || !wasi_bind || !wasi_connect || !wasi_listen || !wasi_local || !wasi_remote || !wasi_accept ||
		!wasi_close || !wasi_sendto || !wasi_recvfrom || !wasi_send || !wasi_set_reuse || !wasi_set_broadcast ||
		!wasi_set_nodelay || !wasi_set_recv_timeout)
		return false;
	static NativeSymbol symbols[] = {
		{"socket", (void *)(uintptr_t)open_socket, "(iii)i", NULL},
		{"bind", (void *)(uintptr_t)bind_socket, "(iii)i", NULL},
		{"connect", (void *)(uintptr_t)connect_socket, "(iii)i", NULL},
		{"listen", (void *)(uintptr_t)listen_socket, "(ii)i", NULL},
		{"getsockname", (void *)(uintptr_t)get_local, "(iii)i", NULL},
		{"getpeername", (void *)(uintptr_t)get_peer, "(iii)i", NULL},
		{"__wrap_accept", (void *)(uintptr_t)accept_socket, "(iii)i", NULL},
		{"sendto", (void *)(uintptr_t)send_to, "(iiiiii)i", NULL},
		{"recvfrom", (void *)(uintptr_t)recv_from, "(iiiiii)i", NULL},
		{"setsockopt", (void *)(uintptr_t)set_option, "(iiiii)i", NULL},
		{"__wrap_getsockopt", (void *)(uintptr_t)get_option, "(iiiii)i", NULL},
	};
	return wasm_runtime_register_natives("env", symbols, sizeof(symbols) / sizeof(symbols[0]));
}
