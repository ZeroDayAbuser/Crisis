const net = require('net');
const { artifactRequest, createIdentity, signedAuthentication } = require('./device-proof');
const { decodeFrame, encodeFrame } = require('../../src/lib/native-protocol');

function deriveXorKey(accessToken, hwid) {
  const seed = accessToken + hwid;
  let hash = 0;
  for (let index = 0; index < seed.length; index++) {
    hash = ((hash << 5) - hash) + seed.charCodeAt(index);
    hash |= 0;
  }
  const bytes = Buffer.alloc(256);
  for (let index = 0; index < bytes.length; index++) {
    bytes[index] = (hash ^ (index * 0x9E)) & 0xFF;
    hash = ((Math.imul(hash, 1103515245) + 12345) >>> 0);
  }
  return bytes;
}

function xorBuffer(buffer, key) {
  for (let index = 0; index < buffer.length; index++) buffer[index] ^= key[index % key.length];
}

function parseEndpoint(value) {
  const text = String(value || '127.0.0.1:223');
  const split = text.lastIndexOf(':');
  if (split <= 0) return { host: text, port: 223 };
  return { host: text.slice(0, split), port: Number(text.slice(split + 1)) || 223 };
}

function nativeCall(endpoint, json) {
  const { host, port } = parseEndpoint(endpoint);
  return new Promise((resolve, reject) => {
    const socket = net.connect({ host, port }, () => {
      socket.end(encodeFrame(json, null, { request: true }));
    });
    const chunks = [];
    socket.on('data', (chunk) => chunks.push(chunk));
    socket.on('error', reject);
    socket.on('end', () => {
      const frame = decodeFrame(Buffer.concat(chunks));
      if (!frame) reject(new Error('Invalid native response'));
      else resolve({ status: frame.json.status, body: frame.json, binary: frame.binary });
    });
  });
}

async function main() {
  const key = process.argv[2] || 'DEMO-KEY-001';
  const hwid = process.argv[3] || 'HWID-123456789';
  const moduleName = process.argv[4] || 'test-dll.dll';
  const endpoint = process.argv[5] || '127.0.0.1:223';

  const identity = createIdentity();
  const auth = await nativeCall(endpoint, signedAuthentication(identity, key, hwid));
  if (auth.status !== 200 || !auth.body.success || typeof auth.body.access_token !== 'string') {
    throw new Error(`Authentication failed with status ${auth.status}: ${auth.body.error || 'invalid response'}`);
  }

  const response = await nativeCall(endpoint, artifactRequest(identity, 'module', moduleName, auth.body.access_token));
  if (response.status !== 200 || !response.body.success) {
    throw new Error(`Module request failed with status ${response.status}: ${response.body.error || 'invalid response'}`);
  }

  const decrypted = Buffer.from(response.binary);
  xorBuffer(decrypted, deriveXorKey(auth.body.access_token, hwid));
  if (decrypted.length < 4) throw new Error(`Module response too short: ${decrypted.length} bytes`);

  const manifestLength = decrypted.readUInt32LE(0);
  const mappedOffset = 4 + manifestLength;
  if (decrypted.length < mappedOffset) throw new Error(`Manifest incomplete: bytes=${decrypted.length}, manifest=${manifestLength}`);
  const mapped = decrypted.subarray(mappedOffset);
  if (mapped.length < 2 || mapped.toString('ascii', 0, 2) !== 'MZ') throw new Error('Mapped module does not start with MZ header');

  console.log(`Module stream OK: ${moduleName}`);
  console.log(`  session-masked bytes: ${response.binary.length}`);
  console.log(`  manifest bytes:       ${manifestLength}`);
  console.log(`  mapped bytes:         ${mapped.length}`);
}

main().catch((error) => {
  console.error(`[ERROR] ${error.message}`);
  process.exit(1);
});
