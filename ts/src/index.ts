import { BufferWriter, BufferReader } from "./buffer.js";

console.log("asdasdasd");

try {
	let x: BufferWriter = new BufferWriter();
	x.addString("A");
	x.addString("B\n");
	//console.log(x.toString());
} catch (e) {
	console.log(e);
}
