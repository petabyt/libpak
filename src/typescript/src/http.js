import * as net from "c:socket";
import { BufferWriter, BufferReader } from "pak:buffer";
import { Module } from "pak:runtime";

export class Http {
	static request(ip, path) {
		 let socket = new HttpSocket(ip);
		 socket.connect();
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
	userAgent = "UserAgent";
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

          rc = net.connect(fd, addr);
          if (rc != 0) throw `connect(${rc})`;

          this.fd = fd;
	}

	close() {
          net.close(this.fd);
	}

     request(path) {
          let content = "";
          let head = this.requestChunks(path, 2000, (buf, totalSize) => {
               for (let i = 0; i < buf.length; i++) {
                    content += String.fromCharCode(buf[i]);
               }
          });
          return [head, content];
     }

     requestChunks(path, maxSize = 2000, handleContent) {
          let writer = new BufferWriter();
          writer.addString("GET " + path + " HTTP/1.1\r\n");
          writer.addString("Accept-Encoding: gzip\r\n");
          //writer.addString("Connection: close\r\n");
          writer.addString("User-Agent: " + this.userAgent + "\r\n");
          writer.addString("Host: " + this.ip + "\r\n");
          writer.addString("\r\n");
          let rc = net.write(this.fd, writer.arrayBuffer, writer.offset);
          if (rc < 0) {
               throw `write(): ${rc}`
          }

          // Read at least header first so we can get content length and send it in chunks
          let buffer = new Uint8Array();
          let reads = 0;
          while (buffer.length < 4096) {
               let temp1 = new Uint8Array(4096);
               rc = net.read(this.fd, temp1.buffer, temp1.length, reads > 1 ? 1 : 2000);
               reads++;
               if (rc < 0) throw `read(): ${rc}`;
               if (rc == 0) break;
               let temp2 = new Uint8Array(rc + buffer.length);
               temp2.set(buffer, 0);
               temp2.set(temp1.subarray(0, rc), buffer.length);
               buffer = temp2;
          }

          let head = "";
          let content = "";
          let i;
          for (i = 0; i < buffer.length; i++) {
               if (buffer[i] == 13 && buffer[i + 2] == 13) { i += 4; break; }
               if (buffer[i] == 13) continue;
               head += String.fromCharCode(buffer[i]);
          }

          let contentLength = 0;
          let props = head.split("\n");
          for (const line in props) {
               if (props[line].startsWith("Content-Length: ")) {
                    contentLength = Number(props[line].substring(16));
               }
          }

          let x = buffer.length - i;
          handleContent(buffer.subarray(i, buffer.length), contentLength);

          let bytesLeft = contentLength - (buffer.length - i);
          buffer = new Uint8Array(maxSize);
          while (contentLength != 0) {
               rc = net.read(this.fd, buffer.buffer, Math.min(bytesLeft, buffer.length), 2000);
               if (rc <= 0) break;
               handleContent(buffer.subarray(0, rc), contentLength);
               bytesLeft -= rc;
          }

          return head;
     }
}
