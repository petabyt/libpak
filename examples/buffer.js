export class BufferReader {
    u8;
    off;
    extendBuffer;
    constructor(buf, byteOffset = 0) {
        this.u8 = buf;
        this.off = byteOffset;
        this.extendBuffer = false;
    }
    get byteLength() { return this.u8.length; }
    get byteOffset() { return this.off; }
    get buffer() { return this.u8; }
    chk(o, n) {
        if (o + n > this.u8.length) {
            throw new RangeError("offset");
        }
    }
    getUint8(o) {
        this.chk(o, 1);
        return this.u8[o];
    }
    getUint16(o) {
        this.chk(o, 2);
        const b0 = this.u8[o];
        const b1 = this.u8[o + 1];
        return (b0 | (b1 << 8));
    }
    getUint32(o) {
        this.chk(o, 4);
        const b0 = this.u8[o];
        const b1 = this.u8[o + 1];
        const b2 = this.u8[o + 2];
        const b3 = this.u8[o + 3];
        return (b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)) >>> 0;
    }
}
export class BufferWriter {
    u8;
    off;
    constructor(size = 0x1000) {
        this.u8 = new Uint8Array(size);
        this.off = 0x0;
    }
    reset() {
        this.off = 0;
    }
    chk(o, n) {
        if (o + n > this.u8.length) {
            let newu8 = new Uint8Array(this.u8.length);
            newu8.set(this.u8);
            this.u8 = newu8;
        }
    }
    setUint8(o, v) {
        this.chk(o, 1);
        this.u8[o] = v;
    }
    setUint16(o, v) {
        this.chk(o, 2);
        this.u8[o] = v & 0xff;
        this.u8[o + 1] = v >> 8;
    }
    setUint32(o, v) {
        this.chk(o, 4);
        this.u8[o] = v & 0xff;
        this.u8[o + 1] = (v >> 8) & 0xff;
        this.u8[o + 2] = (v >> 16) & 0xff;
        this.u8[o + 3] = (v >> 24) & 0xff;
    }
    setString(o, s, nullTerminate = false) {
        let i = 0;
        for (i = 0; i < s.length; i++) {
            this.u8[o + i] = s[i].charCodeAt(i);
        }
        if (nullTerminate) {
            this.u8[o + i] = 0;
            i++;
        }
        return i;
    }
    setOffset(o) {
        this.off = o;
    }
    addString(s, nullTerminate = false) {
        this.off += this.setString(this.off, s, nullTerminate);
    }
}
//# sourceMappingURL=buffer.js.map
