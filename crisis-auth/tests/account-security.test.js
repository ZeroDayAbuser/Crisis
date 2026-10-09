const assert = require('assert/strict');
const fs = require('fs');
const os = require('os');
const path = require('path');

async function main() {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-account-'));
  process.env.DATABASE_PATH = path.join(directory, 'database.json');
  process.env.CRISIS_UPLOAD_ROOT = path.join(directory, 'artifacts');
  process.env.CRISIS_COOKIE_SECURE = 'false';
  fs.writeFileSync(process.env.DATABASE_PATH, JSON.stringify({ keys: { 'LICENSE-ONE': { duration_seconds: 3600, start_unix: 1, expiry_unix: Math.floor(Date.now() / 1000) + 3600, hwid: 'OLD-HWID', client_public_key: 'bound-device-key' } }, players: {} }));
  const { app, store } = require('../server');
  const server = await new Promise((resolve) => { const value = app.listen(0, '127.0.0.1', () => resolve(value)); });
  const base = `http://127.0.0.1:${server.address().port}`;
  const identities = new Map();

  async function request(route, { method = 'GET', body, identity, headers = {}, csrf = true } = {}) {
    const state = identities.get(identity) || { cookie: '', csrf: '' };
    if (state.cookie) headers.Cookie = state.cookie;
    if (!['GET', 'HEAD'].includes(method) && state.csrf && csrf) headers['X-CSRF-Token'] = state.csrf;
    if (body && !(body instanceof FormData)) { headers['Content-Type'] = 'application/json'; body = JSON.stringify(body); }
    const response = await fetch(`${base}${route}`, { method, headers, body });
    const setCookies = response.headers.getSetCookie ? response.headers.getSetCookie() : [];
    if (identity && setCookies.length) {
      const values = {};
      for (const cookie of setCookies) { const [pair] = cookie.split(';'); const split = pair.indexOf('='); values[pair.slice(0, split)] = pair.slice(split + 1); }
      state.cookie = Object.entries(values).filter(([, value]) => value).map(([key, value]) => `${key}=${value}`).join('; ');
      state.csrf = values.crisis_csrf ? decodeURIComponent(values.crisis_csrf) : state.csrf;
      identities.set(identity, state);
    }
    let payload = {}; try { payload = await response.json(); } catch {}
    return { status: response.status, body: payload, headers: response.headers };
  }

  try {
    for (const [email, identity, role] of [['admin@example.com', 'admin', 'admin'], ['user@example.com', 'user', 'admin']]) {
      assert.equal((await request('/api/auth/register', { method: 'POST', body: { email, password: 'Correct horse 42!', role }, identity })).status, 202);
    }
    store.transaction((db) => { const admin = Object.values(db.users).find((user) => user.email === 'admin@example.com'); admin.role = 'admin'; });
    assert.equal((await request('/api/auth/login', { method: 'POST', body: { email: 'admin@example.com', password: 'Correct horse 42!' }, identity: 'admin' })).status, 200);
    assert.equal((await request('/api/auth/login', { method: 'POST', body: { email: 'user@example.com', password: 'Correct horse 42!' }, identity: 'user' })).status, 200);

    const normalAdminAttempt = await request('/api/admin/users', { identity: 'user' });
    assert.equal(normalAdminAttempt.status, 403, 'normal users cannot access admin endpoints');
    const csrfAttempt = await request('/api/account/licenses/redeem', { method: 'POST', body: { license_key: 'LICENSE-ONE' }, identity: 'user', headers: { 'X-CSRF-Token': 'wrong' }, csrf: false });
    assert.equal(csrfAttempt.status, 403, 'state changes reject invalid CSRF');
    assert.equal((await request('/api/account/licenses/redeem', { method: 'POST', body: { license_key: 'LICENSE-ONE' }, identity: 'user' })).status, 200);

    const authorization = await request('/api/device/authorization', { method: 'POST', body: { device_name: 'Test console', scopes: ['account:read'] } });
    assert.equal(authorization.status, 201);
    const review = await request('/api/account/device/review', { method: 'POST', body: { user_code: authorization.body.user_code }, identity: 'user' });
    assert.equal(review.status, 200); assert.deepEqual(review.body.device.scopes, ['account:read']);
    const decisions = await Promise.all([1, 2].map(() => request('/api/account/device/decision', { method: 'POST', body: { user_code: authorization.body.user_code, decision: 'approve' }, identity: 'user' })));
    assert.deepEqual(decisions.map((result) => result.status).sort(), [200, 400], 'authorization can be linked only once');
    const token = await request('/api/device/token', { method: 'POST', body: { device_code: authorization.body.device_code } });
    assert.equal(token.status, 200); assert.equal(token.body.status, 'authorized');
    assert.equal((await request('/api/device/token', { method: 'POST', body: { device_code: authorization.body.device_code } })).status, 400, 'device code replay fails');
    assert.equal((await request('/api/device/account', { headers: { Authorization: `Device ${token.body.device_credential}` } })).status, 200);
    const rotated = await request('/api/device/rotate', { method: 'POST', body: {}, headers: { Authorization: `Device ${token.body.device_credential}` } });
    assert.equal(rotated.status, 200);
    assert.equal((await request('/api/device/account', { headers: { Authorization: `Device ${token.body.device_credential}` } })).status, 401, 'rotated credentials are revoked');
    assert.equal((await request('/api/device/account', { headers: { Authorization: `Device ${rotated.body.device_credential}` } })).status, 200);

    const pending = await request('/api/device/authorization', { method: 'POST', body: { device_name: 'Pending', scopes: ['account:read'] } });
    assert.equal((await request('/api/device/token', { method: 'POST', body: { device_code: pending.body.device_code } })).status, 202);
    assert.equal((await request('/api/device/token', { method: 'POST', body: { device_code: pending.body.device_code } })).status, 429, 'polling interval is enforced');

    const expired = await request('/api/device/authorization', { method: 'POST', body: { device_name: 'Expired', scopes: ['account:read'] } });
    store.transaction((db) => { Object.values(db.device_authorizations).find((entry) => entry.device_name === 'Expired').expires_at = 1; });
    const expiredPoll = await request('/api/device/token', { method: 'POST', body: { device_code: expired.body.device_code } });
    assert.equal(expiredPoll.body.status, 'expired_token');

    const users = await request('/api/admin/users?q=user%40example.com', { identity: 'admin' });
    const ref = users.body.users[0].admin_ref;
    assert.equal(users.body.users[0].role, 'user', 'client-supplied roles are ignored');
    const adjustments = await Promise.all(Array.from({ length: 10 }, () => request(`/api/admin/users/${ref}/coins`, { method: 'POST', body: { delta: 1, reason: 'Concurrency test' }, identity: 'admin' })));
    assert.ok(adjustments.every((result) => result.status === 200));
    const detail = await request(`/api/admin/users/${ref}`, { identity: 'admin' });
    assert.equal(detail.body.user.coin_balance, 10, 'concurrent coin adjustments are not lost');
    assert.equal((await request(`/api/admin/users/${ref}/subscription`, { method: 'POST', body: { seconds: 86400, reason: 'Add one day' }, identity: 'admin' })).status, 200);
    assert.equal((await request(`/api/admin/users/${ref}/hwid-reset`, { method: 'POST', body: { reason: 'New hardware' }, identity: 'admin' })).status, 200);
    assert.equal(store.read().keys['LICENSE-ONE'].client_public_key, null, 'hardware reset also clears the proof-of-possession key');
    const admins = await request('/api/admin/users?q=admin%40example.com', { identity: 'admin' });
    assert.equal((await request(`/api/admin/users/${admins.body.users[0].admin_ref}/status`, { method: 'POST', body: { status: 'suspended', reason: 'Self suspension attempt' }, identity: 'admin' })).status, 409);

    const badUpload = new FormData(); badUpload.set('reason', 'Reject invalid file'); badUpload.set('file', new Blob([Buffer.from('not a PE')], { type: 'application/octet-stream' }), 'bad.dll');
    assert.equal((await request('/api/admin/files/module-test', { method: 'POST', body: badUpload, identity: 'admin' })).status, 415);
    const pe = Buffer.alloc(128); pe.write('MZ'); pe.writeUInt32LE(64, 0x3c); pe.write('PE\0\0', 64, 'binary');
    const goodUpload = new FormData(); goodUpload.set('reason', 'Approved test replacement'); goodUpload.set('file', new Blob([pe], { type: 'application/octet-stream' }), 'replacement.dll');
    assert.equal((await request('/api/admin/files/module-test', { method: 'POST', body: goodUpload, identity: 'admin' })).status, 200);
    assert.ok(fs.existsSync(path.join(process.env.CRISIS_UPLOAD_ROOT, 'current', 'test-dll.dll')));

    assert.equal((await request('/api/auth/register', { method: 'POST', body: { email: 'password@example.com', password: 'Original password 42!' }, identity: 'password' })).status, 202);
    assert.equal((await request('/api/auth/login', { method: 'POST', body: { email: 'password@example.com', password: 'Original password 42!' }, identity: 'password' })).status, 200);
    assert.equal((await request('/api/account/password', { method: 'POST', body: { current_password: 'Original password 42!', new_password: 'Replacement password 84!' }, identity: 'password' })).status, 200);
    assert.equal((await request('/api/auth/login', { method: 'POST', body: { email: 'password@example.com', password: 'Original password 42!' }, identity: 'password' })).status, 401);
    assert.equal((await request('/api/auth/login', { method: 'POST', body: { email: 'password@example.com', password: 'Replacement password 84!' }, identity: 'password' })).status, 200);

    for (let index = 0; index < 11; index++) await request('/api/auth/login', { method: 'POST', body: { email: 'missing@example.com', password: 'wrong' } });
    assert.equal((await request('/api/auth/login', { method: 'POST', body: { email: 'missing@example.com', password: 'wrong' } })).status, 429, 'login rate limit applies');
    console.log('Account, CSRF, RBAC, device, transaction, and upload security tests passed.');
  } finally {
    await new Promise((resolve) => server.close(resolve));
    fs.rmSync(directory, { recursive: true, force: true });
  }
}
main().catch((error) => { console.error(error); process.exit(1); });
