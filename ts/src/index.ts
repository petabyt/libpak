import { WiFi } from "pak:wifi";
import { BufferWriter, BufferReader } from "./buffer.js";

function testWifi() {
	let wifi = new WiFi();
}

function testBuffer() {
	let x: BufferWriter = new BufferWriter();
	x.addString("GET /info HTTP/1.1\r\n");
}
