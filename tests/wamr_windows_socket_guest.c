#include <stdint.h>
#include <string.h>
#include <wasi/api.h>

#define IMPORT(name) __attribute__((import_module("wasi_snapshot_preview1"), import_name(name)))
IMPORT("sock_open") int sock_open(int, int, int, uint32_t *);
IMPORT("sock_connect") int sock_connect(int, void *);
IMPORT("sock_send_to") int sock_send_to(int, void *, int, int, void *, uint32_t *);
IMPORT("sock_recv_from") int sock_recv_from(int, void *, int, int, void *, uint32_t *);
IMPORT("sock_set_recv_timeout") int sock_set_recv_timeout(int, uint64_t);

/* Return 100 + WASI errno on failure; other nonzero values identify the step. */
__attribute__((export_name("probe"))) int probe(int udp, int port) {
    uint32_t fd, count;
    size_t n;
    uint8_t addr[24] = {0}, reply[4] = {0};
    static const uint8_t message[] = "ping";
    __wasi_ciovec_t out[] = {{message, 2}, {message + 2, 2}};
    __wasi_iovec_t in[] = {{reply, 2}, {reply + 2, 2}};
    addr[4] = 127;
    addr[7] = 1;
    addr[8] = port & 255;
    addr[9] = (port >> 8) & 255;
    int error = sock_open(-1, 0, udp ? 0 : 1, &fd);
    if (error)
        return 100 + error;
    error = sock_set_recv_timeout(fd, 2000000);
    if (error)
        return 100 + error;
    if (udp) {
        error = sock_send_to(fd, out, 2, 0, addr, &count);
        if (error)
            return 100 + error;
        if (count != 4)
            return 1;
        error = sock_recv_from(fd, in, 2, 0, addr, &count);
        n = count;
    }
    else {
        error = sock_connect(fd, addr);
        if (error)
            return 100 + error;
        error = __wasi_fd_write(fd, out, 2, &n);
        if (error)
            return 100 + error;
        if (n != 4)
            return 2;
        error = __wasi_fd_read(fd, in, 2, &n);
    }
    if (error)
        return 100 + error;
    if (n != 4 || memcmp(reply, message, 4))
        return 3;
    return __wasi_fd_close(fd);
}
