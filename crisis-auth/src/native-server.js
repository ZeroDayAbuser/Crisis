const net = require('net');
const { createFrameReader, encodeFrame } = require('./lib/native-protocol');

const SOCKET_IDLE_MS = 30_000;
const MAX_CONNECTIONS = 256;

function remoteAddress(socket) {
  return String(socket.remoteAddress || '').replace(/^::ffff:/, '') || 'unknown';
}

function createNativeServer(handleRequest, options = {}) {
  const idleMs = Number.isFinite(options.idleMs) ? options.idleMs : SOCKET_IDLE_MS;
  const maxConnections = Number.isFinite(options.maxConnections) ? options.maxConnections : MAX_CONNECTIONS;

  let openConnections = 0;
  const server = net.createServer((socket) => {
    const ip = remoteAddress(socket);
    if (openConnections >= maxConnections) {
      socket.destroy();
      return;
    }

    openConnections += 1;
    let handled = false;
    const reader = createFrameReader({ request: true });
    socket.setTimeout(idleMs);
    socket.on('timeout', () => socket.destroy());
    socket.on('error', () => socket.destroy());
    socket.on('close', () => { openConnections -= 1; });
    socket.on('data', async (chunk) => {
      if (handled) {
        socket.destroy();
        return;
      }
      const frames = reader(chunk);
      if (frames && frames.error) {
        socket.destroy();
        return;
      }
      if (!frames.length) return;
      handled = true;
      socket.pause();
      try {
        const result = await handleRequest(frames[0], ip);
        const status = Number.isSafeInteger(result?.status) ? result.status : 500;
        const body = result?.body && typeof result.body === 'object' ? result.body : { success: false, error: 'Native request failed' };
        if (body.status === undefined) body.status = status;
        if (body.success === undefined) body.success = status < 400;
        socket.end(encodeFrame(body, result?.binary || null));
      } catch {
        try {
          socket.end(encodeFrame({ success: false, status: 500, error: 'Native request failed' }));
        } catch {
          socket.destroy();
        }
      }
    });
  });
  return server;
}

module.exports = { createNativeServer, remoteAddress };
