import { WiFi } from "pak:wifi";
import * as net from "c:socket";
import { LittleEndianBuffer } from "./buffer.js";

try {
	let wifi = new WiFi();

	let fd = net.socket(net.AF_INET, net.SOCK_STREAM, 0);

	let yes = new ArrayBuffer(4);
	new DataView(yes).setInt32(0, 1, true);
	let rc = net.setsockopt(fd, net.IPPROTO_TCP, net.TCP_NODELAY, yes);
	if (rc) {
		console.log("Failed to set sock opt");
	}

	net.close(fd);

	//let adapters = wifi.getAdapters();
	let adapter = wifi.getDefaultAdapter();
} catch (e) {
	console.log(e);
}
