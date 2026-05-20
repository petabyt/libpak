import * as net from "c:socket";
import { BufferWriter, BufferReader } from "pak:buffer";

export class Http {
    request(ip, path) {
         let socket = HttpSocket(ip);
         let r = socket.request(path);
         socket.close();
         return r;
    }
}

function hexdump(data) {
	let str = "";
	for (let i = 0; i < data.length; i++) {
		str += data[i].toString(16) + " ";
	}
	console.log(str);
}

export class HttpSocket {
    ip;
    fd;
    port;
    constructor(ip) {
         this.ip = ip;
         this.port = 80;
    }

    connect(bindFunction = function(fd) {}) {
         let fd = net.socket(net.AF_INET, net.SOCK_STREAM, net.IPPROTO_TCP);
         if (fd < 0) throw "socket()";

         bindFunction(fd);

         let yes = net.createInt(1);
         let rc = net.setsockopt(fd, net.IPPROTO_TCP, net.TCP_NODELAY, yes);
         if (rc) throw "setsockopt()";

         let addr = net.createSockAddrIn(this.ip, this.port);

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
         writer.addString("\r\n");
         let rc = net.write(this.fd, writer.arrayBuffer, writer.offset);
         if (rc < 0) {
              throw "write(): " + String(rc);
         }
         let readBuffer = new Uint8Array();
         while (true) {
         	let buf = new Uint8Array(2000);
         	rc = net.read(this.fd, buf.buffer, buf.length);
         	if (rc <= 0) break;
         	let temp = new Uint8Array(rc + readBuffer.length);
         	temp.set(readBuffer, 0);
         	temp.set(buf.subarray(0, rc), readBuffer.length);
         	readBuffer = temp;
        }
		let head = "";
		let content = "";
		let i;
		for (i = 0; i < readBuffer.length - 4; i++) {
			if (readBuffer[i] == 13 && readBuffer[i + 2] == 13) break;
			if (readBuffer[i] == 13) continue;
			head += String.fromCharCode(readBuffer[i]);
		}
		for (i += 2; i < readBuffer.length; i++) {
			if (readBuffer[i] == 13) continue;
			content += String.fromCharCode(readBuffer[i]);
		}
		return head;
    }
}
