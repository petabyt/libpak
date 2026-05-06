import * as net from "c:socket";
import { BufferWriter, BufferReader } from "pak:buffer";

export class Http {
	function request(ip, path) {
		let socket = HttpSocket(ip);
		let r = socket.request(path);
		socket.close();
		return r;
	}
}

export class HttpSocket {
	ip;
	fd;
	constructor(ip) {
		this.ip = ip;
	}

	connect(bindFunction = function(fd) {}) {
		let fd = net.socket(net.AF_INET, net.SOCK_STREAM, net.IPPROTO_TCP);
		if (fd < 0) throw "socket()";

		bindFunction(fd);

		let yes = net.createInt(1);
		let rc = net.setsockopt(fd, net.IPPROTO_TCP, net.TCP_NODELAY, yes);
		if (rc) throw "setsockopt()";

		let addr = net.createSockAddrIn(this.ip, 80);

		if (net.connect(fd, addr) != 0) throw "connect()";

		this.fd = fd;
	}

	close() {
		net.close(this.fd);
	}

	request(path) {
		let writer = new BufferWriter();
		writer.addString("GET " + path + " HTTP/1.1\r\n");
		writer.addString("Accept-Encoding: gzip\r\n");
		writer.addString("Connection: close\r\n");
		writer.addString("User-Agent: " + this.userAgent + "\r\n");
		writer.addString("Host: " + this.ip + "\r\n");
		let rc = net.write(this.fd, writer.arrayBuffer, writer.offset);
		if (rc < 0) {
			throw "write(): " + String(rc);
		}
		let read = new BufferReader(new Uint8Array(1000));
		rc = net.read(this.fd, read.arrayBuffer, read.buffer.length);
		if (rc < 0) {
			throw "read()";
		}
		read.offset = rc;
		let head = read.toString();
		rc = net.read(this.fd, read.arrayBuffer, read.buffer.length);
		if (rc < 0) {
			throw "read()";
		}
		read.offset = rc;
		let contents = read.toString();
		return [head, contents];
	}
}
