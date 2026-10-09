const assert = require('assert/strict');
const fs = require('fs');
const http = require('http');
const os = require('os');
const path = require('path');
const test = require('node:test');
const { createStore } = require('../src/lib/data-store');

function request(port, headers = {}) {
  return new Promise((resolve, reject) => {
    const call = http.get({ hostname: '127.0.0.1', port, path: '/', headers }, (response) => {
      const chunks = [];
      response.on('data', (chunk) => chunks.push(chunk));
      response.on('end', () => resolve({ status: response.statusCode, body: JSON.parse(Buffer.concat(chunks).toString('utf8')) }));
    });
    call.on('error', reject);
  });
}

test('production rejects plaintext requests and accepts trusted HTTPS proxy requests', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-https-'));
  process.env.NODE_ENV = 'production';
  process.env.DATABASE_PATH = path.join(directory, 'database.json');
  createStore(process.env.DATABASE_PATH).initialize();
  const { app } = require('../server');
  const server = await new Promise((resolve) => { const value = app.listen(0, '127.0.0.1', () => resolve(value)); });
  try {
    const plaintext = await request(server.address().port);
    assert.equal(plaintext.status, 426);
    assert.equal(plaintext.body.error, 'HTTPS is required');

    const proxiedTls = await request(server.address().port, { 'X-Forwarded-Proto': 'https' });
    assert.equal(proxiedTls.status, 405);
    assert.match(proxiedTls.body.error, /TCP/);
  } finally {
    await new Promise((resolve, reject) => server.close((error) => error ? reject(error) : resolve()));
    fs.rmSync(directory, { recursive: true, force: true });
    delete process.env.NODE_ENV;
    delete process.env.DATABASE_PATH;
  }
});
