const assert = require('assert/strict');
const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const test = require('node:test');

test('signed DM admin generate interaction receives an immediate response', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-discord-route-'));
  const adminId = '222222222222222222';
  const { publicKey, privateKey } = crypto.generateKeyPairSync('ed25519');
  process.env.DATABASE_PATH = path.join(directory, 'database.json');
  process.env.CRISIS_DISCORD_PUBLIC_KEY = publicKey.export({ format: 'der', type: 'spki' }).subarray(-32).toString('hex');
  process.env.CRISIS_DISCORD_ADMIN_USER_IDS = adminId;
  const { app } = require('../server');
  const server = await new Promise((resolve) => { const value = app.listen(0, '127.0.0.1', () => resolve(value)); });
  try {
    const interaction = {
      type: 2,
      context: 1,
      data: { name: 'generate', options: [{ name: 'length', type: 4, value: 10 }, { name: 'unit', type: 3, value: 'days' }, { name: 'game', type: 3, value: 'rust' }] },
      user: { id: adminId, username: 'dm-admin', global_name: 'DM Admin', avatar: null }
    };
    const body = JSON.stringify(interaction);
    const timestamp = String(Math.floor(Date.now() / 1000));
    const signature = crypto.sign(null, Buffer.concat([Buffer.from(timestamp), Buffer.from(body)]), privateKey).toString('hex');
    const response = await fetch(`http://127.0.0.1:${server.address().port}/api/discord/interactions`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'X-Signature-Ed25519': signature, 'X-Signature-Timestamp': timestamp },
      body
    });
    const payload = await response.json();
    assert.equal(response.status, 200);
    assert.equal(payload.type, 4);
    assert.match(payload.data.content, /Generated 10 days/);
    assert.equal(Object.keys(require('../src/lib/data-store').createStore(process.env.DATABASE_PATH).read().keys).length, 1);
    assert.deepEqual(Object.values(require('../src/lib/data-store').createStore(process.env.DATABASE_PATH).read().keys)[0].games, ['rust']);
  } finally {
    await new Promise((resolve, reject) => server.close((error) => error ? reject(error) : resolve()));
    fs.rmSync(directory, { recursive: true, force: true });
    delete process.env.DATABASE_PATH;
    delete process.env.CRISIS_DISCORD_PUBLIC_KEY;
    delete process.env.CRISIS_DISCORD_ADMIN_USER_IDS;
  }
});
