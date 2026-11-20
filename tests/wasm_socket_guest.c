#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <runtime.h>
#if defined(TEST_WIFI_BIND)
#include <wifi.h>
#endif

static int test(struct PakModule *mod, int job) {
	(void)mod; (void)job;
	errno = 0;
	if (socket(AF_UNIX, SOCK_STREAM, 0) != -1 || errno != EAFNOSUPPORT) return 1;
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) return 2;
#if defined(TEST_WIFI_BIND)
	struct PakNet *net = pak_net_get_context();
	struct PakWiFiAdapter *adapter = pak_wifi_get_adapter(net, 0);
	if (!net || !adapter || pak_wifi_bind_socket_to_adapter(net, adapter, fd)) return 26;
	if (pak_wifi_bind_socket_to_adapter(net, adapter, -1) != -1) return 27;
	if (pak_wifi_bind_socket_to_adapter(net, adapter, 1) != -1) return 28;
#endif
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return 3;
	if (close(fd)) return 4;
#if defined(TEST_WIFI_BIND)
	if (pak_wifi_bind_socket_to_adapter(net, adapter, fd) != -1) return 29;
	if (pak_wifi_unref_adapter(net, adapter)) return 30;
#endif
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0 || close(fd)) return 5;
	int server = socket(AF_INET, SOCK_STREAM, 0);
	if (server < 0) return 6;
	int yes = 1;
	if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))) return 7;
	struct sockaddr_in a = {0}, peer = {0};
	a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(0x7f000001);
	if (bind(server, (struct sockaddr *)&a, sizeof(a))) return 8;
	socklen_t size = sizeof(a);
	if (getsockname(server, (struct sockaddr *)&a, &size) || !a.sin_port || size != sizeof(a)) return 9;
	if (listen(server, 1)) return 10;
	int client = socket(AF_INET, SOCK_STREAM, 0);
	if (client < 0) return 11;
#if defined(TEST_SO_ERROR)
	flags = fcntl(client, F_GETFL, 0);
	if (flags < 0 || fcntl(client, F_SETFL, flags | O_NONBLOCK)) return 31;
	int rc = connect(client, (struct sockaddr *)&a, sizeof(a));
	if (rc && errno != EINPROGRESS) return 32;
	fd_set wfds; FD_ZERO(&wfds); FD_SET(client, &wfds);
	struct timeval timeout = {1, 0};
	if (select(client + 1, NULL, &wfds, NULL, &timeout) != 1) return 33;
	int so_error = -1; socklen_t optlen = sizeof(so_error);
	if (getsockopt(client, SOL_SOCKET, SO_ERROR, &so_error, &optlen) || so_error || optlen != sizeof(so_error)) return 34;
	optlen = sizeof(so_error); errno = 0;
	if (getsockopt(-1, SOL_SOCKET, SO_ERROR, &so_error, &optlen) != -1 || errno != EBADF) return 41;
	optlen = sizeof(so_error); errno = 0;
	if (getsockopt(1, SOL_SOCKET, SO_ERROR, &so_error, &optlen) != -1 || (errno != ENOTSOCK && errno != EBADF)) return 42;
#else
	if (connect(client, (struct sockaddr *)&a, sizeof(a))) return 11;
#endif
	size = sizeof(peer);
	int accepted = accept(server, (struct sockaddr *)&peer, &size);
	if (accepted < 0 || peer.sin_family != AF_INET || !peer.sin_port) return 12;
	char c = 'x', d = 0;
	if (write(client, &c, 1) != 1 || read(accepted, &d, 1) != 1 || d != c) return 13;
	if (close(client) || close(accepted) || close(server)) return 14;
#if defined(TEST_SO_ERROR)
	client = socket(AF_INET, SOCK_STREAM, 0);
	if (client < 0 || fcntl(client, F_SETFL, O_NONBLOCK)) return 35;
	rc = connect(client, (struct sockaddr *)&a, sizeof(a));
	if (rc && errno != EINPROGRESS) return 36;
	FD_ZERO(&wfds); FD_SET(client, &wfds); timeout.tv_sec = 1; timeout.tv_usec = 0;
	rc = select(client + 1, NULL, &wfds, NULL, &timeout);
	if (rc != 1) return rc < 0 ? 100 + errno : 37;
	so_error = 0; optlen = sizeof(so_error);
	if (getsockopt(client, SOL_SOCKET, SO_ERROR, &so_error, &optlen) || so_error != ECONNREFUSED) return 38;
	so_error = -1; optlen = sizeof(so_error);
	if (getsockopt(client, SOL_SOCKET, SO_ERROR, &so_error, &optlen) || so_error) return 39;
	if (close(client)) return 40;
#endif
	int receiver = socket(AF_INET, SOCK_DGRAM, 0);
	int sender = socket(AF_INET, SOCK_DGRAM, 0);
	if (receiver < 0 || sender < 0) return 15;
	if (setsockopt(sender, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes))) return 16;
	a.sin_port = 0;
	if (bind(receiver, (struct sockaddr *)&a, sizeof(a))) return 17;
	size = sizeof(a);
	if (getsockname(receiver, (struct sockaddr *)&a, &size)) return 18;
	if (sendto(sender, &c, 1, 0, (struct sockaddr *)&a, sizeof(a)) != 1) return 19;
	fd_set fds; FD_ZERO(&fds); FD_SET(receiver, &fds);
	struct timeval tv = {1, 0};
	if (select(receiver + 1, &fds, NULL, NULL, &tv) != 1) return 20;
	size = sizeof(peer); d = 0;
	if (recvfrom(receiver, &d, 1, 0, (struct sockaddr *)&peer, &size) != 1 || d != c || !peer.sin_port) return 21;
	if (connect(sender, (struct sockaddr *)&a, sizeof(a))) return 23;
	if (sendto(sender, &c, 1, 0, NULL, 0) != 1) return 24;
	d = 0;
	if (recvfrom(receiver, &d, 1, 0, NULL, NULL) != 1 || d != c) return 25;
	if (close(receiver) || close(sender)) return 22;
	return 0;
}

int get_module(struct PakModule *mod) { mod->on_run_test = test; return 0; }
