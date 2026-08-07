/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef _WASI_SOCKET_EXT_H_
#define _WASI_SOCKET_EXT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

#ifndef SO_DEBUG
#define SO_DEBUG        1
#define SO_REUSEADDR    2
#define SO_TYPE         3
#define SO_ERROR        4
#define SO_DONTROUTE    5
#define SO_BROADCAST    6
#define SO_SNDBUF       7
#define SO_RCVBUF       8
#define SO_KEEPALIVE    9
#define SO_OOBINLINE    10
#define SO_NO_CHECK     11
#define SO_PRIORITY     12
#define SO_LINGER       13
#define SO_BSDCOMPAT    14
#define SO_REUSEPORT    15
#define SO_PASSCRED     16
#define SO_PEERCRED     17
#define SO_RCVLOWAT     18
#define SO_SNDLOWAT     19
#define SO_ACCEPTCONN   30
#define SO_PEERSEC      31
#define SO_SNDBUFFORCE  32
#define SO_RCVBUFFORCE  33
#define SO_PROTOCOL     38
#define SO_DOMAIN       39
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /* Used only for sock_addr_resolve hints */
    SOCKET_ANY = -1,
    SOCKET_DGRAM = 0,
    SOCKET_STREAM,
} __wasi_sock_type_t;

typedef uint16_t __wasi_ip_port_t;

typedef enum { IPv4 = 0, IPv6 } __wasi_addr_type_t;

/*
 n0.n1.n2.n3
 Example:
  IP Address: 127.0.0.1
  Structure: {n0: 127, n1: 0, n2: 0, n3: 1}
*/
typedef struct __wasi_addr_ip4_t {
    uint8_t n0;
    uint8_t n1;
    uint8_t n2;
    uint8_t n3;
} __wasi_addr_ip4_t;

typedef struct __wasi_addr_ip4_port_t {
    __wasi_addr_ip4_t addr;
    __wasi_ip_port_t port; /* host byte order */
} __wasi_addr_ip4_port_t;

/*
 n0:n1:n2:n3:h0:h1:h2:h3, each 16bit value uses host byte order
 Example (little-endian system)
  IP Address fe80::3ba2:893b:4be0:e3dd
  Structure: {
    n0: 0xfe80, n1:0x0, n2: 0x0, n3: 0x0,
    h0: 0x3ba2, h1: 0x893b, h2: 0x4be0, h3: 0xe3dd
  }
*/
typedef struct __wasi_addr_ip6_t {
    uint16_t n0;
    uint16_t n1;
    uint16_t n2;
    uint16_t n3;
    uint16_t h0;
    uint16_t h1;
    uint16_t h2;
    uint16_t h3;
} __wasi_addr_ip6_t;

typedef struct __wasi_addr_ip6_port_t {
    __wasi_addr_ip6_t addr;
    __wasi_ip_port_t port; /* host byte order */
} __wasi_addr_ip6_port_t;

typedef struct __wasi_addr_ip_t {
    __wasi_addr_type_t kind;
    union {
        __wasi_addr_ip4_t ip4;
        __wasi_addr_ip6_t ip6;
    } addr;
} __wasi_addr_ip_t;

typedef struct __wasi_addr_t {
    __wasi_addr_type_t kind;
    union {
        __wasi_addr_ip4_port_t ip4;
        __wasi_addr_ip6_port_t ip6;
    } addr;
} __wasi_addr_t;

typedef enum { INET4 = 0, INET6, INET_UNSPEC } __wasi_address_family_t;

typedef struct __wasi_addr_info_t {
    __wasi_addr_t addr;
    __wasi_sock_type_t type;
} __wasi_addr_info_t;

typedef struct __wasi_addr_info_hints_t {
    __wasi_sock_type_t type;
    __wasi_address_family_t family;
    // this is to workaround lack of optional parameters
    uint8_t hints_enabled;
} __wasi_addr_info_hints_t;

#ifdef __wasi__
/**
 * Reimplement below POSIX APIs with __wasi_sock_XXX functions.
 *
 * Keep sync with
 * <sys/socket.h>
 * <sys/types.h>
 */
#define SO_REUSEADDR 2
#define SO_BROADCAST 6
#define SO_SNDBUF 7
#define SO_RCVBUF 8
#define SO_KEEPALIVE 9
#define SO_LINGER 13
#define SO_REUSEPORT 15
#define SO_RCVTIMEO 20
#define SO_SNDTIMEO 21

#define TCP_NODELAY 1
#define TCP_KEEPIDLE 4
#define TCP_KEEPINTVL 5
#define TCP_QUICKACK 12
#define TCP_FASTOPEN_CONNECT 30

#define IP_TTL 2
#define IP_MULTICAST_TTL 33
#define IP_MULTICAST_LOOP 34
#define IP_ADD_MEMBERSHIP 35
#define IP_DROP_MEMBERSHIP 36

#define IPV6_MULTICAST_LOOP 19
#define IPV6_JOIN_GROUP 20
#define IPV6_LEAVE_GROUP 21
#define IPV6_V6ONLY 26

/* getaddrinfo error codes.
 *
 * we use values compatible with wasi-libc/musl netdb.h.
 * https://github.com/WebAssembly/wasi-libc/blob/4ea6fdfa288e15a57c02fe31dda78e5ddc87c3c7/libc-top-half/musl/include/netdb.h#L43-L53
 *
 * for now, non-posix error codes are excluded:
 * EAI_PROTOCOL and EAI_BADHINTS (BSDs)
 * EAI_ADDRFAMILY, EAI_NODATA
 * https://github.com/WebAssembly/wasi-libc/blob/4ea6fdfa288e15a57c02fe31dda78e5ddc87c3c7/libc-top-half/musl/include/netdb.h#L145-L152
 */

#define EAI_AGAIN -3
#define EAI_BADFLAGS -1
#define EAI_FAIL -4
#define EAI_FAMILY -6
#define EAI_MEMORY -10
#define EAI_NONAME -2
#define EAI_OVERFLOW -12
#define EAI_SERVICE -8
#define EAI_SOCKTYPE -7
#define EAI_SYSTEM -11

struct addrinfo {
    int ai_flags;             /* Input flags.  */
    int ai_family;            /* Protocol family for socket.  */
    int ai_socktype;          /* Socket type.  */
    int ai_protocol;          /* Protocol for socket.  */
    socklen_t ai_addrlen;     /* Length of socket address.  */
    struct sockaddr *ai_addr; /* Socket address for socket.  */
    char *ai_canonname;       /* Canonical name for service location.  */
    struct addrinfo *ai_next; /* Pointer to next in list.  */
};

#ifndef __WASI_RIGHTS_SOCK_ACCEPT
int
accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
#endif

int
bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int
connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int
listen(int sockfd, int backlog);

ssize_t
recvmsg(int sockfd, struct msghdr *msg, int flags);

ssize_t
sendmsg(int sockfd, const struct msghdr *msg, int flags);

ssize_t
sendto(int sockfd, const void *buf, size_t len, int flags,
       const struct sockaddr *dest_addr, socklen_t addrlen);

ssize_t
recvfrom(int sockfd, void *buf, size_t len, int flags,
         struct sockaddr *src_addr, socklen_t *addrlen);

int
socket(int domain, int type, int protocol);

int
getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

int
getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

int
getsockopt(int sockfd, int level, int optname, void *__restrict optval,
           socklen_t *__restrict optlen);

int
setsockopt(int sockfd, int level, int optname, const void *optval,
           socklen_t optlen);

int
getaddrinfo(const char *node, const char *service, const struct addrinfo *hints,
            struct addrinfo **res);

void
freeaddrinfo(struct addrinfo *res);

const char *
gai_strerror(int code);
#endif

#ifdef __cplusplus
}
#endif

#endif
