export class BufferReader {
	private buffer: Uint8Array;
	private off: number;
	private extendBuffer: boolean;

	constructor(buf: Uint8Array, byteOffset = 0) {
		this.buffer = buf;
		this.off = byteOffset;
		this.extendBuffer = false;
	}

	get byteLength() { return this.buffer.length; }
	get byteOffset() { return this.off; }

	private chk(o: number, n: number) {
		if (o + n > this.buffer.length) {
			throw new RangeError("offset");
		}
	}

	getUint8(o: number): number {
		this.chk(o, 1);
		return this.buffer[o];
	}

	getUint16(o: number): number {
		this.chk(o, 2);
		const b0 = this.buffer[o];
		const b1 = this.buffer[o + 1];
		return (b0 | (b1 << 8));
	}

	getUint32(o: number): number {
		this.chk(o, 4);
		const b0 = this.buffer[o];
		const b1 = this.buffer[o + 1];
		const b2 = this.buffer[o + 2];
		const b3 = this.buffer[o + 3];

		return (b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)) >>> 0;
	}
}

export class BufferWriter {
	private buffer: Uint8Array;
	private off: number;

	constructor(size: number = 0x1000) {
		this.buffer = new Uint8Array(size);
		this.off = 0x0;
	}

	private chk(o: number, n: number) {
		if (o + n > this.buffer.length) {
			let newbuffer = new Uint8Array(this.buffer.length);
			newbuffer.set(this.buffer);
			this.buffer = newbuffer;
		}
	}

	setUint8(o: number, v: number) {
		this.chk(o, 1);
		this.buffer[o] = v;
	}

	setUint16(o: number, v: number) {
		this.chk(o, 2);
		this.buffer[o] = v & 0xff;
		this.buffer[o + 1] = v >> 8;
	}

	setUint32(o: number, v: number) {
		this.chk(o, 4);
		this.buffer[o] = v & 0xff;
		this.buffer[o + 1] = (v >> 8) & 0xff;
		this.buffer[o + 2] = (v >> 16) & 0xff;
		this.buffer[o + 3] = (v >> 24) & 0xff;
	}

	setString(o: number, s: string, nullTerminate = false): number {
		let i = 0;
		for (i = 0; i < s.length; i++) {
			this.buffer[o + i] = s[i].charCodeAt(i);
		}
		if (nullTerminate) {
			this.buffer[o + i] = 0;
			i++;
		}
		return i;
	}

	setOffset(o: number) {
		this.off = o;
	}

	addString(s: string, nullTerminate = false) {
		this.off += this.setString(this.off, s, nullTerminate);
	}
}
