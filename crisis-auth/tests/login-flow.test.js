const assert = require('assert/strict');
const crypto = require('crypto');
const fs = require('fs');
const http = require('http');
const net = require('net');
const os = require('os');
const path = require('path');

const { artifactCanonical, authCanonical } = require('../src/lib/client-proof');
const { decodeFrame, encodeFrame } = require('../src/lib/native-protocol');
const { bindPlayerToLicense } = require('../src/player-service');

function clientIdentity() {
  const pair = crypto.generateKeyPairSync('ec', { namedCurve: 'prime256v1' });
  const jwk = pair.publicKey.export({ format: 'jwk' });
  const blob = Buffer.alloc(72);
  blob.writeUInt32LE(0x31534345, 0);
  blob.writeUInt32LE(32, 4);
  Buffer.from(jwk.x, 'base64url').copy(blob, 8);
  Buffer.from(jwk.y, 'base64url').copy(blob, 40);
  return { pair, publicKey: blob.toString('base64url') };
}

function signedAuthentication(identity, key, hwid, extra = {}) {
  const timestamp_unix = Math.floor(Date.now() / 1000);
  const nonce = crypto.randomBytes(32).toString('base64url');
  const signature = crypto.sign('sha256', Buffer.from(authCanonical(key, hwid, timestamp_unix, nonce)), {
    key: identity.pair.privateKey, dsaEncoding: 'ieee-p1363'
  }).toString('base64url');
  return { op: 'authenticate', key, hwid, client_public_key: identity.publicKey, timestamp_unix, nonce, signature, ...extra };
}

function artifactRequest(identity, op, name, accessToken) {
  const timestamp_unix = Math.floor(Date.now() / 1000);
  const nonce = crypto.randomBytes(32).toString('base64url');
  const signature = crypto.sign('sha256', Buffer.from(artifactCanonical(op, name, accessToken, timestamp_unix, nonce)), {
    key: identity.pair.privateKey, dsaEncoding: 'ieee-p1363'
  }).toString('base64url');
  return { op, name, access_token: accessToken, timestamp_unix, nonce, signature };
}

function nativeCall(port, json) {
  return new Promise((resolve, reject) => {
    const socket = net.connect({ host: '127.0.0.1', port }, () => {
      socket.end(encodeFrame(json, null, { request: true }));
    });
    const chunks = [];
    socket.on('data', (chunk) => chunks.push(chunk));
    socket.on('error', reject);
    socket.on('end', () => {
      const frame = decodeFrame(Buffer.concat(chunks));
      if (!frame) return reject(new Error('invalid native response'));
      resolve({ status: frame.json.status, body: frame.json, binary: frame.binary });
    });
  });
}

function get(port, urlPath, headers = {}) {
  return new Promise((resolve, reject) => {
    const request = http.get({ hostname: '127.0.0.1', port, path: urlPath, headers }, (response) => {
      const chunks = [];
      response.on('data', (chunk) => chunks.push(chunk));
      response.on('end', () => resolve({ status: response.statusCode, body: JSON.parse(Buffer.concat(chunks).toString('utf8')) }));
    });
    request.on('error', reject);
  });
}

function post(port, urlPath, body, headers = {}) {
  return new Promise((resolve, reject) => {
    const payload = JSON.stringify(body);
    const request = http.request({ hostname: '127.0.0.1', port, method: 'POST', path: urlPath, headers: {
      'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(payload), ...headers
    } }, (response) => {
      const chunks = [];
      response.on('data', (chunk) => chunks.push(chunk));
      response.on('end', () => resolve({ status: response.statusCode, body: JSON.parse(Buffer.concat(chunks).toString('utf8')) }));
    });
    request.on('error', reject);
    request.end(payload);
  });
}

async function main() {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-login-'));
  const databasePath = path.join(directory, 'database.json');
  fs.writeFileSync(databasePath, JSON.stringify({ keys: {
    'LOGIN-KEY': { duration_seconds: 3600, games: ['fortnite'], start_unix: null, expiry_unix: null, hwid: null, discord_id: '111111111111111111', created_at: 1 },
    'PAIRING-KEY': { duration_seconds: 3600, games: ['rust'], start_unix: null, expiry_unix: null, hwid: null, created_at: 1 }
  } }));
  const uploadRoot = path.join(directory, 'storage');
  const moduleDirectory = path.join(uploadRoot, 'current');
  fs.mkdirSync(moduleDirectory, { recursive: true });
  fs.copyFileSync(path.join(__dirname, '..', 'modules', 'test-dll.dll'), path.join(moduleDirectory, 'crisis-fortnite.dll'));
  fs.copyFileSync(path.join(__dirname, '..', 'modules', 'test-dll.dll'), path.join(moduleDirectory, 'crisis-rust.dll'));
  process.env.CRISIS_UPLOAD_ROOT = uploadRoot;
  process.env.DATABASE_PATH = databasePath;
  const { app, startNativeServer, store } = require('../server');
  const { pauseLicense, resumeLicense } = require('../src/lib/license-state');
  const server = await new Promise((resolve) => { const value = app.listen(0, '127.0.0.1', () => resolve(value)); });
  const nativeServer = startNativeServer(0);
  if (!nativeServer.listening) await new Promise((resolve) => nativeServer.once('listening', resolve));
  const port = server.address().port;
  const nativePort = nativeServer.address().port;
  const loginIdentity = clientIdentity();
  const pairingIdentity = clientIdentity();
  const attackerIdentity = clientIdentity();
  try {
    let result = await get(port, '/?key=LOGIN-KEY&hwid=HWID');
    assert.equal(result.status, 405, 'license credentials are not accepted in URLs');

    result = await post(port, '/api/client/authenticate', { key: 'LOGIN-KEY', hwid: 'HWID' });
    assert.equal(result.status, 410, 'HTTP authentication is retired in favor of TCP');
    assert.equal((await get(port, '/module?key=LOGIN-KEY&hwid=HWID&name=test-dll.dll')).status, 410,
      'HTTP module downloads are retired in favor of TCP');
    assert.equal((await get(port, '/driver?key=LOGIN-KEY&hwid=HWID&name=driver.sys')).status, 410,
      'HTTP driver downloads are retired in favor of TCP');

    result = await nativeCall(nativePort, { op: 'authenticate', key: 'LOGIN-KEY', hwid: 'HWID' });
    assert.equal(result.status, 401, 'a key and spoofable HWID cannot authenticate without device proof');
    result = await nativeCall(nativePort, signedAuthentication(loginIdentity, 'LOGIN-KEY', 'HWID'));
    assert.equal(result.status, 200);
    assert.equal(result.body.success, true);
    assert.deepEqual(result.body.games, ['fortnite']);
    assert.match(result.body.access_token, /^[A-Za-z0-9_-]{43}$/);
    assert.equal(result.body.token_type, 'Bearer');
    assert.ok(result.body.access_token_expires_in > 0 && result.body.access_token_expires_in <= 300);
    assert.match(result.body.player_token, /^[A-Za-z0-9_-]{43}$/);
    assert.equal(result.body.player_token_type, 'Bearer');
    assert.equal(result.body.player_token_expires_unix, result.body.expiry_unix);
    assert.ok(result.body.expiry_unix > result.body.start_unix);
    const firstAccessToken = result.body.access_token;
    const firstPlayerToken = result.body.player_token;

    assert.equal((await get(port, '/v1/player', { Authorization: `Bearer ${firstAccessToken}` })).status, 401,
      'artifact access tokens cannot authorize player routes');
    result = await get(port, '/v1/player', { Authorization: `Bearer ${firstPlayerToken}` });
    assert.equal(result.status, 200);
    assert.equal(result.body.player.discord_id, '111111111111111111');
    const playerTcp = await nativeCall(nativePort, { op: 'player', player_token: firstPlayerToken });
    assert.equal(playerTcp.status, 200);
    assert.equal(playerTcp.body.player.discord_id, '111111111111111111');
    assert.equal((await nativeCall(nativePort, { op: 'player', player_token: firstAccessToken })).status, 401,
      'artifact tokens cannot authorize player TCP routes');
    const casinoState = await nativeCall(nativePort, { op: 'casino.state', player_token: firstPlayerToken });
    assert.equal(casinoState.status, 200);
    assert.equal(casinoState.body.success, true);

    assert.equal((await get(port, '/v1/player?key=LOGIN-KEY&hwid=HWID')).status, 401,
      'static credentials cannot authorize player routes');
    assert.equal((await nativeCall(nativePort, { op: 'driver', name: 'driver.sys', access_token: firstAccessToken })).status, 401,
      'a stolen bearer token cannot download artifacts without a fresh device signature');
    const driver = await nativeCall(nativePort, artifactRequest(loginIdentity, 'driver', 'driver.sys', firstAccessToken));
    assert.equal(driver.status, 200);
    assert.ok(driver.binary.length > 0);
    const moduleName = 'test-dll.dll';
    const module = await nativeCall(nativePort, artifactRequest(loginIdentity, 'module', moduleName, firstAccessToken));
    assert.equal(module.status, 200);
    assert.equal((await nativeCall(nativePort, artifactRequest(loginIdentity, 'module', moduleName, firstPlayerToken))).status, 401,
      'player tokens cannot authorize artifact downloads');
    assert.ok(module.binary.length > 4);
    assert.equal((await nativeCall(nativePort, artifactRequest(loginIdentity, 'module', 'crisis-fortnite.dll', firstAccessToken))).status, 200);
    assert.equal((await nativeCall(nativePort, artifactRequest(loginIdentity, 'module', 'crisis-rust.dll', firstAccessToken))).status, 403,
      'Fortnite-only keys cannot download Rust modules');
    const replay = artifactRequest(loginIdentity, 'module', moduleName, firstAccessToken);
    assert.equal((await nativeCall(nativePort, replay)).status, 200);
    assert.equal((await nativeCall(nativePort, replay)).status, 401, 'artifact proofs cannot be replayed');
    assert.equal((await nativeCall(nativePort, signedAuthentication(attackerIdentity, 'LOGIN-KEY', 'HWID'))).status, 401,
      'the license remains bound to the original device key even when its license key and HWID are known');
    const rotated = await nativeCall(nativePort, signedAuthentication(loginIdentity, 'LOGIN-KEY', 'HWID'));
    assert.equal(rotated.status, 200);
    assert.notEqual(rotated.body.access_token, firstAccessToken);
    assert.notEqual(rotated.body.player_token, firstPlayerToken);
    assert.equal((await get(port, '/v1/player', { Authorization: `Bearer ${firstPlayerToken}` })).status, 401,
      'issuing a new token pair revokes the previous player token');
    const activeAccessToken = rotated.body.access_token;
    const activePlayerToken = rotated.body.player_token;

    result = await nativeCall(nativePort,
      signedAuthentication(pairingIdentity, 'PAIRING-KEY', 'PAIRING-HWID', { discord_id: '222222222222222222' }));
    assert.equal(result.status, 400, 'the native client cannot self-assert a Discord identity');
    assert.equal(store.read().keys['PAIRING-KEY'].start_unix, null);

    result = await nativeCall(nativePort, signedAuthentication(pairingIdentity, 'PAIRING-KEY', 'PAIRING-HWID'));
    assert.equal(result.status, 200);
    assert.match(result.body.pairing_code, /^[A-Z2-9]{4}-[A-Z2-9]{4}-[A-Z2-9]{4}$/);
    assert.ok(result.body.pairing_expires_in > 3500 && result.body.pairing_expires_in <= 3600);
    const pairingCode = result.body.pairing_code;
    result = await nativeCall(nativePort, signedAuthentication(pairingIdentity, 'PAIRING-KEY', 'PAIRING-HWID'));
    assert.equal(result.body.pairing_code, pairingCode, 're-authentication must not rotate an unconsumed pairing code');
    const pairingPlayerToken = result.body.player_token;
    assert.equal((await nativeCall(nativePort, {
      op: 'casino.state', player_token: pairingPlayerToken
    })).status, 401, 'an unlinked license cannot use another Discord account balance');
    store.transaction((db) => {
      const linked = bindPlayerToLicense(db, 'PAIRING-KEY', '222222222222222222',
        { username: 'paired-user', display_name: 'Paired User', avatar_url: '' });
      assert.ok(linked.player);
    });
    const pairedCasino = await nativeCall(nativePort, {
      op: 'casino.state', player_token: pairingPlayerToken
    });
    assert.equal(pairedCasino.status, 200);
    assert.equal(pairedCasino.body.success, true, 'the existing player token becomes usable immediately after pairing');

    store.transaction((db) => pauseLicense(db.keys['LOGIN-KEY'], 'test-pause'));
    result = await nativeCall(nativePort, signedAuthentication(loginIdentity, 'LOGIN-KEY', 'HWID'));
    assert.equal(result.status, 423);
    assert.equal(result.body.error, 'Subscription is paused');
    assert.equal((await get(port, '/v1/player', { Authorization: `Bearer ${activePlayerToken}` })).status, 401);
    store.transaction((db) => resumeLicense(db.keys['LOGIN-KEY'], 'test-pause'));

    const renewed = await nativeCall(nativePort, signedAuthentication(loginIdentity, 'LOGIN-KEY', 'HWID'));
    assert.equal(renewed.status, 200);
    assert.notEqual(renewed.body.access_token, activeAccessToken);
    assert.notEqual(renewed.body.player_token, activePlayerToken);

    result = await post(port, '/admin/create-key', { key: 'ADMIN-TEST', duration_seconds: 60, reason: 'Login flow test' });
    assert.equal(result.status, 503);
    process.env.CRISIS_ADMIN_SECRET = 'test-admin-secret-that-is-at-least-32-bytes';
    result = await post(port, '/admin/create-key', { key: 'ADMIN-TEST', duration_seconds: 60, reason: 'Login flow test' },
      { Authorization: 'Bearer wrong-secret-that-is-at-least-32-bytes' });
    assert.equal(result.status, 403);
    result = await post(port, '/admin/create-key', { key: 'ADMIN-TEST', duration_seconds: 60, games: ['fortnite', 'rust'], reason: 'Login flow test' },
      { Authorization: `Bearer ${process.env.CRISIS_ADMIN_SECRET}` });
    assert.equal(result.status, 200);
    assert.equal(result.body.success, true);
    assert.deepEqual(store.read().keys['ADMIN-TEST'].games, ['fortnite', 'rust']);
    console.log('Authenticated login flow tests passed.');
  } finally {
    await new Promise((resolve, reject) => nativeServer.close((error) => error ? reject(error) : resolve()));
    await new Promise((resolve, reject) => server.close((error) => error ? reject(error) : resolve()));
    fs.rmSync(directory, { recursive: true, force: true });
    delete process.env.CRISIS_ADMIN_SECRET;
    delete process.env.CRISIS_UPLOAD_ROOT;
  }
}

main().catch((error) => { console.error(error); process.exit(1); });
