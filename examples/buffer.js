export class LittleEndianBuffer {
  constructor(buf, byte_offset = 0) {
    this.u8 = buf;
    this.off = byte_offset;
    this.len = buf.length;
  }
  get byteLength() {
    return this.len;
  }
  get byteOffset() {
    return this.off;
  }
  get buffer() {
    return this.u8;
  }
  chk(o, n) {
    if (o < 0 || o + n > this.len)
      throw new RangeError("offset");
  }
  getUint8(o) {
    this.chk(o, 1);
    return this.u8[o];
  }
  getUint16(o) {
    this.chk(o, 2);
    const b0 = this.u8[o];
    const b1 = this.u8[o + 1];
    return b0 | b1 << 8;
  }
  getUint32(o) {
    this.chk(o, 4);
    const b0 = this.u8[o];
    const b1 = this.u8[o + 1];
    const b2 = this.u8[o + 2];
    const b3 = this.u8[o + 3];
    return (b0 | b1 << 8 | b2 << 16 | b3 << 24) >>> 0;
  }
}
