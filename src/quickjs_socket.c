#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>
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
};

static JSValue generic_operation(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst argv[], int magic) {
	switch (magic) {
	case M_SOCKET: {
		int32_t domain, type, protocol;
		JS_ToInt32(ctx, &domain, argv[0]);
		JS_ToInt32(ctx, &type, argv[0]);
		JS_ToInt32(ctx, &protocol, argv[0]);
		return JS_NewInt32(ctx, socket(domain, type, protocol));
		}
	case M_SETSOCKOPT: {
		int32_t fd, level, optname;
		size_t len;
		JS_ToInt32(ctx, &fd, argv[0]);
		JS_ToInt32(ctx, &level, argv[1]);
		JS_ToInt32(ctx, &optname, argv[2]);
		void *optval = JS_GetArrayBuffer(ctx, &len, argv[3]);
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
	case M_SELECT: {
		int32_t nfds;
		size_t len;
		fd_set *r, *w, *e;
		JS_ToInt32(ctx, &nfds, argv[0]);
		r = JS_GetOpaque(argv[1], 0);
		w = JS_GetOpaque(argv[2], 0);
		e = JS_GetOpaque(argv[3], 0);
		struct timeval *tv = (void *)JS_GetArrayBuffer(ctx, &len, argv[4]);
		return JS_NewInt32(ctx, select(nfds, r, w, e, tv));
		}
	}
	return JS_UNDEFINED;
}

#define JS_CONSTANT(x) JS_PROP_INT32_DEF(#x, x, JS_PROP_CONFIGURABLE)
static const JSCFunctionListEntry socket_methods[] = {
	JS_CFUNC_MAGIC_DEF("socket", 3, generic_operation, M_SOCKET),
	JS_CFUNC_MAGIC_DEF("setsockopt", 3, generic_operation, M_SOCKET),
	JS_CFUNC_MAGIC_DEF("getsockopt", 3, generic_operation, M_SOCKET),
	JS_CFUNC_MAGIC_DEF("inet_pton", 3, generic_operation, M_SOCKET),
	JS_CFUNC_MAGIC_DEF("connect", 3, generic_operation, M_SOCKET),
	JS_CFUNC_MAGIC_DEF("close", 3, generic_operation, M_SOCKET),
	JS_CFUNC_MAGIC_DEF("select", 3, generic_operation, M_SOCKET),

	JS_CONSTANT(SOL_SOCKET),
	JS_CONSTANT(IPPROTO_TCP),
	JS_CONSTANT(TCP_NODELAY),
	JS_CONSTANT(O_NONBLOCK),
	JS_CONSTANT(SO_RCVTIMEO),
	JS_CONSTANT(SO_REUSEADDR),
	JS_CONSTANT(SO_ERROR),
	JS_CONSTANT(F_SETFL),
	JS_CONSTANT(AF_INET),
};

static int module_init(JSContext* ctx, JSModuleDef *m) {
	JS_SetModuleExportList(ctx, m, socket_methods, sizeof(socket_methods) / sizeof(socket_methods[0]));
	return 0;
}

JSModuleDef *js_init_module_socket(JSContext *ctx, const char *module_name) {
	JSModuleDef *m = JS_NewCModule(ctx, module_name, module_init);
	if (!m) return NULL;
	return m;
}
