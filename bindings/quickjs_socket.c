#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#ifdef WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
#else
	#include <sys/socket.h>
	#include <sys/select.h>
	#include <netinet/tcp.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
#endif

enum Operations {
	M_SOCKET,
	M_SETSOCKOPT,
	M_GETSOCKOPT,
	M_INET_PTON,
	M_CONNECT,
	M_CLOSE,
	M_SELECT,
	M_READ,
	M_WRITE,
	M_CREATESOCKADDRIN,
	M_CREATEINT,
};

static fd_set *parse_fd_set(JSContext *ctx, fd_set *set, JSValue arr) {
	if (JS_IsArray(ctx, arr)) {
		FD_ZERO(set);
		for (int i = 0; 1; i++) {
			uint32_t n;
			JSValue p = JS_GetPropertyUint32(ctx, arr, 0);
			if (!JS_IsNumber(p)) break;
			JS_ToUint32(ctx, &n, p);
			FD_SET((int)n, set);
			JS_FreeValue(ctx, p);
		}
	} else {
		return NULL;
	}
	return set;
}

static JSValue generic_operation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[], int magic) {
	switch (magic) {
	case M_SOCKET: {
		int32_t domain, type, protocol;
		JS_ToInt32(ctx, &domain, argv[0]);
		JS_ToInt32(ctx, &type, argv[1]);
		JS_ToInt32(ctx, &protocol, argv[2]);
		return JS_NewInt32(ctx, socket(domain, type, protocol));
		}
	case M_SETSOCKOPT: {
		int32_t fd, level, optname;
		size_t len;
		JS_ToInt32(ctx, &fd, argv[0]);
		JS_ToInt32(ctx, &level, argv[1]);
		JS_ToInt32(ctx, &optname, argv[2]);
		void *optval = JS_GetArrayBuffer(ctx, &len, argv[3]);
		if (optval == NULL) abort();
		return JS_NewInt32(ctx, setsockopt(fd, level, optname, optval, len));
	}
	case M_GETSOCKOPT: {
		int32_t fd, level, optname;
		socklen_t len;
		JS_ToInt32(ctx, &fd, argv[0]);
		JS_ToInt32(ctx, &level, argv[1]);
		JS_ToInt32(ctx, &optname, argv[2]);
		void *buf = JS_GetArrayBuffer(ctx, (size_t *)&len, argv[3]);
		return JS_NewInt32(ctx, getsockopt(fd, level, optname, buf, &len));
	}
	case M_INET_PTON: {
		int32_t af;
		const char *src;
		size_t len;
		JS_ToInt32(ctx, &af, argv[0]);
		src = JS_ToCString(ctx, argv[1]);
		void *dst = JS_GetArrayBuffer(ctx, &len, argv[2]);
		int r = inet_pton(af, src, dst);
		JS_FreeCString(ctx, src);
		return JS_NewInt32(ctx, r);
	}
	case M_CONNECT: {
		int32_t fd;
		size_t len;
		JS_ToInt32(ctx, &fd, argv[0]);
		struct sockaddr *sa = (void *)JS_GetArrayBuffer(ctx, &len, argv[1]);
		return JS_NewInt32(ctx, connect(fd, sa, len));
		}
	case M_CLOSE: {
		int32_t fd;
		JS_ToInt32(ctx, &fd, argv[0]);
		return JS_NewInt32(ctx, close(fd));
		}
	case M_WRITE: {
		int32_t fd;
		uint32_t size;
		size_t len;
		JS_ToInt32(ctx, &fd, argv[0]);
		JS_ToUint32(ctx, &size, argv[2]);
		void *src = JS_GetArrayBuffer(ctx, &len, argv[1]);
		if (src == NULL) JS_ThrowInternalError(ctx, "arg");
		return JS_NewInt32(ctx, write(fd, src, size));
		}
	case M_READ: {
		int32_t fd;
		uint32_t size;
		size_t len;
		JS_ToInt32(ctx, &fd, argv[0]);
		void *src = JS_GetArrayBuffer(ctx, &len, argv[1]);
		if (src == NULL) JS_ThrowInternalError(ctx, "arg");
		JS_ToUint32(ctx, &size, argv[2]);
		if (size > len) return JS_NewInt32(ctx, EPIPE);
		return JS_NewInt32(ctx, read(fd, src, size));
		}
	case M_SELECT: {
		int32_t nfds;
		size_t len;
		fd_set rs, ws, es;
		fd_set *r = NULL, *w = NULL, *e = NULL;
		JS_ToInt32(ctx, &nfds, argv[0]);
		r = parse_fd_set(ctx, &rs, argv[1]);
		w = parse_fd_set(ctx, &ws, argv[2]);
		e = parse_fd_set(ctx, &es, argv[3]);
		struct timeval *tv = (void *)JS_GetArrayBuffer(ctx, &len, argv[4]);
		return JS_NewInt32(ctx, select(nfds, r, w, e, tv));
		}
	case M_CREATESOCKADDRIN: {
		uint32_t port = 0;
		const char *addr = JS_ToCString(ctx, argv[0]);
		JS_ToUint32(ctx, &port, argv[1]);
		struct sockaddr_in sa;
		memset(&sa, 0, sizeof(sa));
		sa.sin_family = AF_INET;
		sa.sin_port = htons(port);
		if (inet_pton(AF_INET, addr, &(sa.sin_addr)) <= 0) {
			return JS_ThrowInternalError(ctx, "inet_pton");
		}
		JSValue val = JS_NewArrayBufferCopy(ctx, (void *)&sa, sizeof(sa));
		return val;
		}
	case M_CREATEINT: {
		int32_t n;
		JS_ToInt32(ctx, &n, argv[0]);
		return JS_NewArrayBufferCopy(ctx, (void *)&n, sizeof(n));
		}
	}
	return JS_UNDEFINED;
}

static JSValue get_yes(JSContext *ctx, JSValueConst this_val) {
	int yes = 1;
	printf("Get yes\n");
	return JS_NewArrayBufferCopy(ctx, (const unsigned char *)&yes, sizeof(yes));
}

static JSValue dummy_setter(JSContext *ctx, JSValueConst this_val, JSValueConst val) {
	printf("set\n");
	return JS_ThrowTypeError(ctx, "readonly");
}

#define JS_CONSTANT(x) JS_PROP_INT32_DEF(#x, x, JS_PROP_CONFIGURABLE)
static const JSCFunctionListEntry socket_methods[] = {
	//JS_CGETSET_DEF("yes", get_yes, dummy_setter),

	JS_CFUNC_MAGIC_DEF("socket", 3, generic_operation, M_SOCKET),
	JS_CFUNC_MAGIC_DEF("setsockopt", 4, generic_operation, M_SETSOCKOPT),
	JS_CFUNC_MAGIC_DEF("getsockopt", 3, generic_operation, M_GETSOCKOPT),
	JS_CFUNC_MAGIC_DEF("inet_pton", 3, generic_operation, M_INET_PTON),
	JS_CFUNC_MAGIC_DEF("connect", 3, generic_operation, M_CONNECT),
	JS_CFUNC_MAGIC_DEF("close", 1, generic_operation, M_CLOSE),
	JS_CFUNC_MAGIC_DEF("select", 5, generic_operation, M_SELECT),
	JS_CFUNC_MAGIC_DEF("read", 3, generic_operation, M_READ),
	JS_CFUNC_MAGIC_DEF("write", 3, generic_operation, M_WRITE),
	JS_CFUNC_MAGIC_DEF("createSockAddrIn", 2, generic_operation, M_CREATESOCKADDRIN),
	JS_CFUNC_MAGIC_DEF("createInt", 2, generic_operation, M_CREATEINT),

	JS_CONSTANT(SOL_SOCKET),

	JS_CONSTANT(SO_ACCEPTCONN),
	JS_CONSTANT(SO_BROADCAST),
	JS_CONSTANT(SO_DEBUG),
	JS_CONSTANT(SO_DONTROUTE),
	JS_CONSTANT(SO_ERROR),
	JS_CONSTANT(SO_KEEPALIVE),
	JS_CONSTANT(SO_LINGER),
	JS_CONSTANT(SO_OOBINLINE),
	JS_CONSTANT(SO_RCVBUF),
	JS_CONSTANT(SO_RCVLOWAT),
	JS_CONSTANT(SO_RCVTIMEO),
	JS_CONSTANT(SO_REUSEADDR),
	JS_CONSTANT(SO_REUSEPORT),
	JS_CONSTANT(SO_SNDBUF),
	JS_CONSTANT(SO_SNDLOWAT),
	JS_CONSTANT(SO_SNDTIMEO),
	JS_CONSTANT(SO_TYPE),

	JS_CONSTANT(AF_UNSPEC),
	JS_CONSTANT(AF_INET),
	JS_CONSTANT(AF_INET6),
	JS_CONSTANT(AF_UNIX),
	JS_CONSTANT(AF_LOCAL),

	JS_CONSTANT(PF_INET),
	JS_CONSTANT(PF_INET6),
	JS_CONSTANT(PF_UNIX),

	JS_CONSTANT(SOCK_STREAM),
	JS_CONSTANT(SOCK_DGRAM),
	JS_CONSTANT(SOCK_RAW),
	JS_CONSTANT(SOCK_SEQPACKET),
	JS_CONSTANT(SOCK_RDM),

	JS_CONSTANT(IPPROTO_IP),
	JS_CONSTANT(IPPROTO_TCP),
	JS_CONSTANT(IPPROTO_UDP),
	JS_CONSTANT(IPPROTO_ICMP),
	JS_CONSTANT(IPPROTO_IPV6),

	JS_CONSTANT(TCP_NODELAY),
	JS_CONSTANT(TCP_MAXSEG),
	JS_CONSTANT(TCP_KEEPIDLE),
	JS_CONSTANT(TCP_KEEPINTVL),
	JS_CONSTANT(TCP_KEEPCNT),
	JS_CONSTANT(TCP_INFO),
	JS_CONSTANT(TCP_CORK),
	JS_CONSTANT(TCP_QUICKACK),

	JS_CONSTANT(IP_TTL),
	JS_CONSTANT(IP_TOS),
	JS_CONSTANT(IP_MULTICAST_IF),
	JS_CONSTANT(IP_MULTICAST_TTL),
	JS_CONSTANT(IP_MULTICAST_LOOP),
	JS_CONSTANT(IP_ADD_MEMBERSHIP),
	JS_CONSTANT(IP_DROP_MEMBERSHIP),

	JS_CONSTANT(IPV6_V6ONLY),
	JS_CONSTANT(IPV6_UNICAST_HOPS),
	JS_CONSTANT(IPV6_MULTICAST_IF),
	JS_CONSTANT(IPV6_MULTICAST_HOPS),
	JS_CONSTANT(IPV6_MULTICAST_LOOP),
	JS_CONSTANT(IPV6_JOIN_GROUP),
	JS_CONSTANT(IPV6_LEAVE_GROUP),

	JS_CONSTANT(MSG_OOB),
	JS_CONSTANT(MSG_PEEK),
	JS_CONSTANT(MSG_DONTROUTE),
	JS_CONSTANT(MSG_CTRUNC),
	JS_CONSTANT(MSG_TRUNC),
	JS_CONSTANT(MSG_DONTWAIT),
	JS_CONSTANT(MSG_NOSIGNAL),

	JS_CONSTANT(SHUT_RD),
	JS_CONSTANT(SHUT_WR),
	JS_CONSTANT(SHUT_RDWR),

	JS_CONSTANT(O_NONBLOCK),
	JS_CONSTANT(O_CLOEXEC),

	JS_CONSTANT(F_GETFL),
	JS_CONSTANT(F_SETFL),
	JS_CONSTANT(F_GETFD),
	JS_CONSTANT(F_SETFD),
	JS_CONSTANT(FD_CLOEXEC),

	JS_CONSTANT(EINPROGRESS),
	JS_CONSTANT(EWOULDBLOCK),
	JS_CONSTANT(EAGAIN),
	JS_CONSTANT(ECONNRESET),
	JS_CONSTANT(ECONNREFUSED),
	JS_CONSTANT(ETIMEDOUT),
	JS_CONSTANT(EPIPE),
	JS_CONSTANT(ENOTCONN),
	JS_CONSTANT(EISCONN),
	JS_CONSTANT(EADDRINUSE),
	JS_CONSTANT(EADDRNOTAVAIL),
	JS_CONSTANT(ENETDOWN),
	JS_CONSTANT(ENETUNREACH),
	JS_CONSTANT(EHOSTUNREACH)
};

static int module_init(JSContext* ctx, JSModuleDef *m) {
	JS_SetModuleExportList(ctx, m, socket_methods, sizeof(socket_methods) / sizeof(socket_methods[0]));
	return 0;
}

JSModuleDef *js_init_module_socket(JSContext *ctx, const char *module_name) {
	JSModuleDef *m = JS_NewCModule(ctx, module_name, module_init);
	if (!m) return NULL;
	JS_AddModuleExportList(ctx, m, socket_methods, sizeof(socket_methods) / sizeof(socket_methods[0]));
	return m;
}
