const MAGIC = Buffer.from('CAUT', 'ascii');
const VERSION = 1;
const FLAG_BINARY = 0x01;
const HEADER_SIZE = 16;
const MAX_REQUEST_JSON = 32 * 1024;
const MAX_RESPONSE_JSON = 64 * 1024;
const MAX_BINARY = 64 * 1024 * 1024;

function encodeFrame(json, binary = null, { request = false } = {}) {
  const payload = Buffer.from(JSON.stringify(json), 'utf8');
  const blob = binary && binary.length ? Buffer.from(binary) : Buffer.alloc(0);
  const jsonLimit = request ? MAX_REQUEST_JSON : MAX_RESPONSE_JSON;
  if (payload.length === 0 || payload.length > jsonLimit) throw new Error('Native frame JSON is invalid');
  if (blob.length > MAX_BINARY) throw new Error('Native frame binary is too large');

  const header = Buffer.alloc(HEADER_SIZE);
  MAGIC.copy(header, 0);
  header.writeUInt8(VERSION, 4);
  header.writeUInt8(blob.length ? FLAG_BINARY : 0, 5);
  header.writeUInt16LE(0, 6);
  header.writeUInt32LE(payload.length, 8);
  header.writeUInt32LE(blob.length, 12);
  return Buffer.concat([header, payload, blob]);
}

function decodeHeader(buffer) {
  if (!Buffer.isBuffer(buffer) || buffer.length < HEADER_SIZE) return null;
  if (buffer.compare(MAGIC, 0, 4, 0, 4) !== 0 || buffer.readUInt8(4) !== VERSION) return null;
  const flags = buffer.readUInt8(5);
  if (buffer.readUInt16LE(6) !== 0) return null;
  if (flags & ~FLAG_BINARY) return null;
  const jsonLength = buffer.readUInt32LE(8);
  const binaryLength = buffer.readUInt32LE(12);
  if (!jsonLength || jsonLength > MAX_RESPONSE_JSON || binaryLength > MAX_BINARY) return null;
  if (binaryLength && !(flags & FLAG_BINARY)) return null;
  if (!binaryLength && (flags & FLAG_BINARY)) return null;
  return { jsonLength, binaryLength, total: HEADER_SIZE + jsonLength + binaryLength };
}

function decodeFrame(buffer, { request = false } = {}) {
  const header = decodeHeader(buffer);
  if (!header || buffer.length !== header.total) return null;
  const jsonLimit = request ? MAX_REQUEST_JSON : MAX_RESPONSE_JSON;
  if (header.jsonLength > jsonLimit) return null;
  if (request && header.binaryLength) return null;

  let json;
  try {
    json = JSON.parse(buffer.subarray(HEADER_SIZE, HEADER_SIZE + header.jsonLength).toString('utf8'));
  } catch {
    return null;
  }
  if (!json || typeof json !== 'object' || Array.isArray(json)) return null;
  const binary = header.binaryLength
    ? Buffer.from(buffer.subarray(HEADER_SIZE + header.jsonLength))
    : Buffer.alloc(0);
  return { json, binary };
}

function createFrameReader({ request = false } = {}) {
  let pending = Buffer.alloc(0);
  return function push(chunk) {
    if (!chunk || !chunk.length) return [];
    pending = pending.length ? Buffer.concat([pending, chunk]) : Buffer.from(chunk);
    const frames = [];
    while (pending.length >= HEADER_SIZE) {
      const header = decodeHeader(pending.subarray(0, HEADER_SIZE));
      if (!header) {
        pending = Buffer.alloc(0);
        return { error: 'Invalid native frame' };
      }
      if (pending.length < header.total) return frames;
      const encoded = pending.subarray(0, header.total);
      pending = pending.subarray(header.total);
      const frame = decodeFrame(encoded, { request });
      if (!frame) return { error: 'Invalid native frame' };
      frames.push(frame);
    }
    return frames;
  };
}

function isLoopbackAddress(address) {
  const value = String(address || '');
  return value === '127.0.0.1' || value === '::1' || value === '::ffff:127.0.0.1';
}

module.exports = {
  FLAG_BINARY,
  HEADER_SIZE,
  MAGIC,
  MAX_BINARY,
  MAX_REQUEST_JSON,
  MAX_RESPONSE_JSON,
  VERSION,
  createFrameReader,
  decodeFrame,
  decodeHeader,
  encodeFrame,
  isLoopbackAddress
};
