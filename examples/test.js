// QuickJS test
import { WiFi } from "pak:wifi";
import * as net from "c:socket";
import * as std from "qjs:std";
import { BufferWriter } from "pak:buffer";

function hexdump(data) {
	let str = "";
	for (let i = 0; i < data.length; i++) {
		str += data[i].toString(16) + " ";
	}
	console.log(str);
}

try {
	let wifi = new WiFi();

	let writer = new BufferWriter();
	writer.addString("Hello, World\n");
	writer.addString("Ok", true);
	hexdump(writer.buffer.slice(0, writer.offset));

	//let adapters = wifi.getAdapters();
	let adapter = wifi.getDefaultAdapter();
	console.log("Adapter name: ", adapter.name);
} catch (e) {
	console.log(e);
}
