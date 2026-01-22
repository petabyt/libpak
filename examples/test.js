// QuickJS test
import { WiFi } from "pak:wifi";
import * as net from "c:socket";
import * as std from "qjs:std";
import { BufferWriter } from "../ts/dist/buffer.js";

try {
	let wifi = new WiFi();

	let writer = new BufferWriter();
	writer.addString("Hello, World");

	//let adapters = wifi.getAdapters();
	let adapter = wifi.getDefaultAdapter();
	console.log("Adapter name: ", adapter.name);
} catch (e) {
	console.log(e);
}
