import { WiFi } from "pak:wifi";
import * as net from "c:socket";
import { LittleEndianBuffer } from "./buffer.js";

function getUserAgent() {
	return "xxx";
}

function makeCInt(val) {
	let yes = new ArrayBuffer(4);
	new DataView(yes).setInt32(0, val, true);
	return yes;
}

function httpGetRequest(path, params) {
	let req = "GET " + path + " HTTP/1.1\r\n"
	+ "Accept-Encoding: gzip\r\n"
	+ "Connection: close\r\n"
	+ "User-Agent: " + getUserAgent() + "\r\n"
	+ "Host: 192.168.169.1\r\n";
}

try {
	let wifi = new WiFi();

	let fd = net.socket(net.AF_INET, net.SOCK_STREAM, 0);

	let yes = makeCInt(1);
	let rc = net.setsockopt(fd, net.IPPROTO_TCP, net.TCP_NODELAY, yes);
	if (rc) {
		console.log("Failed to set sock opt");
	}

	net.close(fd);

	//let adapters = wifi.getAdapters();
	let adapter = wifi.getDefaultAdapter();
	console.log("Adapter name: ", adapter.name);
} catch (e) {
	console.log(e);
}
