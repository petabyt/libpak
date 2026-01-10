// Interact with Veecar/Veement dashcams
#include <stdio.h>
//#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <wifi.h>
#include <bluetooth.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int connected(struct PakNet *ctx, struct PakWiFiAdapter *adapter, void *arg) {

	printf("Connected\n");

	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	printf("pak_wifi_bind_socket_to_adapter: %d\n", pak_wifi_bind_socket_to_adapter(ctx, adapter, fd));


	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(80);
	if (inet_pton(AF_INET, "192.168.169.1", &(sa.sin_addr)) <= 0) {
		printf("inet_pton\n");
	}

	if (connect(fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) == -1) {
		printf("connect\n");
	}

	const char *request = "GET /app/getdeviceattr HTTP/1.1\r\n"
	"Accept-Encoding: gzip\r\n"
	"Connection: close\r\n"
	"User-Agent: PakUserAgent\r\n"
	"Host: 192.168.169.1\r\n";

	write(fd, request, strlen(request));

	usleep(1000);

	char buffer[1000];
	printf("%u\n", read(fd, buffer, sizeof(buffer)));
	printf("%u\n", read(fd, buffer, sizeof(buffer)));
	printf("%s\n", buffer);

	close(fd);
	
	return 0;
}

int main(void) {
	struct PakNet *ctx = pak_net_get_context();

	struct PakWiFiApFilter spec = {
		.has_ssid = 1,
		.ssid_pattern = "V300.*",
		.has_password = 1,
		.password = "12345678",
	};

	pak_wifi_request_connection(ctx, &spec, connected, NULL);

	return 0;
}
