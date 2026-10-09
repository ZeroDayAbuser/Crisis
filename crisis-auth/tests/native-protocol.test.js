const assert = require('assert/strict');
const net = require('net');
const test = require('node:test');
const {
  HEADER_SIZE,
  createFrameReader,
  decodeFrame,
  encodeFrame,
  isLoopbackAddress
} = require('../src/lib/native-protocol');

test('native frames round-trip JSON and optional binary payloads', () => {
  const jsonOnly = encodeFrame({ op: 'authenticate', ok: true }, null, { request: true });
  const decoded = decodeFrame(jsonOnly, { request: true });
  assert.equal(decoded.json.op, 'authenticate');
  assert.equal(decoded.binary.length, 0);

  const withBinary = encodeFrame({ success: true }, Buffer.from('MZ'));
  const binary = decodeFrame(withBinary);
  assert.equal(binary.json.success, true);
  assert.equal(binary.binary.toString('latin1'), 'MZ');
});

test('native frame reader rejects HTTP and truncated magic', () => {
  const reader = createFrameReader({ request: true });
  const error = reader(Buffer.from('GET / HTTP/1.1\r\n'));
  assert.equal(error.error, 'Invalid native frame');

  const partial = createFrameReader({ request: true });
  assert.deepEqual(partial(Buffer.from('CA')), []);
});

test('native frame reader reassembles split packets', () => {
  const frame = encodeFrame({ op: 'authenticate', key: 'LOGIN-KEY-1' }, null, { request: true });
  const reader = createFrameReader({ request: true });
  assert.deepEqual(reader(frame.subarray(0, 4)), []);
  assert.deepEqual(reader(frame.subarray(4, HEADER_SIZE + 3)), []);
  const complete = reader(frame.subarray(HEADER_SIZE + 3));
  assert.equal(complete.length, 1);
  assert.equal(complete[0].json.op, 'authenticate');
});

test('loopback detection covers IPv4-mapped IPv6', () => {
  assert.equal(isLoopbackAddress('127.0.0.1'), true);
  assert.equal(isLoopbackAddress('::1'), true);
  assert.equal(isLoopbackAddress('::ffff:127.0.0.1'), true);
  assert.equal(isLoopbackAddress('8.8.8.8'), false);
});

test('native server answers one request per TCP connection', async () => {
  const { createNativeServer } = require('../src/native-server');
  const server = createNativeServer((frame) => {
    assert.equal(frame.json.op, 'ping');
    return { status: 200, body: { success: true, pong: true } };
  });
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const port = server.address().port;
  try {
    const response = await new Promise((resolve, reject) => {
      const socket = net.connect({ host: '127.0.0.1', port }, () => {
        socket.end(encodeFrame({ op: 'ping' }, null, { request: true }));
      });
      const chunks = [];
      socket.on('data', (chunk) => chunks.push(chunk));
      socket.on('error', reject);
      socket.on('end', () => resolve(decodeFrame(Buffer.concat(chunks))));
    });
    assert.equal(response.json.success, true);
    assert.equal(response.json.pong, true);
    assert.equal(response.json.status, 200);
  } finally {
    await new Promise((resolve, reject) => server.close((error) => error ? reject(error) : resolve()));
  }
});
