const express = require('express');
const crypto = require('crypto');
const path = require('path');
const fs = require('fs');
const multer = require('multer');
const {
  clientIp, createRateLimiter, digest, hashPassword, metadata, normalizeEmail,
  randomToken, requestId, safeEqual, validatePassword, verifyPassword
} = require('./lib/security');
const { isLicensePaused, remainingSeconds } = require('./lib/license-state');

const SESSION_SECONDS = 12 * 60 * 60;
const DEVICE_CODE_SECONDS = 10 * 60;
const DEVICE_POLL_SECONDS = 5;
const USER_CODE_ALPHABET = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
const DEVICE_SCOPES = new Set(['account:read', 'artifacts:read']);
const FILE_SLOTS = Object.freeze({
  driver: { extension: '.sys', filename: 'driver.sys' },
  'module-rust': { extension: '.dll', filename: 'rust.dll' },
  'module-test': { extension: '.dll', filename: 'test-dll.dll' }
});

function nowUnix() { return Math.floor(Date.now() / 1000); }
function licenseDigest(key) { return digest(key, 'crisis-license-link:v1'); }
function cookieValue(req, name) {
  for (const part of String(req.get('cookie') || '').split(';')) {
    const separator = part.indexOf('=');
    if (separator > 0 && part.slice(0, separator).trim() === name) return decodeURIComponent(part.slice(separator + 1));
  }
  return null;
}
function userCode() {
  let value = '';
  for (let index = 0; index < 8; index++) value += USER_CODE_ALPHABET[crypto.randomInt(USER_CODE_ALPHABET.length)];
  return `${value.slice(0, 4)}-${value.slice(4)}`;
}
function publicDevice(record) {
  return { device_id: record.public_id, name: record.name, created_at: record.created_at, last_used_at: record.last_used_at || null, revoked_at: record.revoked_at || null, scopes: record.scopes };
}
function linkedLicenses(db, userId) {
  const digests = new Set(Object.values(db.account_license_links || {}).filter((link) => link.user_id === userId).map((link) => link.license_digest));
  return Object.entries(db.keys || {}).filter(([key]) => digests.has(licenseDigest(key)));
}
function accountState(db, user) {
  const licenses = linkedLicenses(db, user.id);
  const now = nowUnix();
  let expiry = Number.isSafeInteger(user.subscription_expiry) ? user.subscription_expiry : null;
  let remaining = expiry ? Math.max(0, expiry - now) : 0;
  let subscriptionPaused = false;
  let coins = Number.isSafeInteger(user.coin_balance) ? user.coin_balance : 0;
  for (const [, license] of licenses) {
    if (Number.isSafeInteger(license.expiry_unix)) expiry = Math.max(expiry || 0, license.expiry_unix);
    remaining = Math.max(remaining, remainingSeconds(license, now));
    subscriptionPaused ||= isLicensePaused(db, license);
    if (license.discord_id && db.players?.[license.discord_id] && Number.isSafeInteger(db.players[license.discord_id].crisis_coins)) coins = db.players[license.discord_id].crisis_coins;
  }
  return {
    email: user.email, email_verified: Boolean(user.email_verified), status: user.status,
    subscription_expiry: expiry, remaining_seconds: remaining, subscription_paused: subscriptionPaused,
    coin_balance: coins, linked_license_count: licenses.length
  };
}

function mountAccountService(app, options) {
  const projectRoot = path.join(__dirname, '..');
  const store = options.store;
  const rate = createRateLimiter();
  const secureCookie = options.secureCookie !== false;
  const verificationUrl = options.verificationUrl || '/device';
  const allowedOrigins = new Set(options.allowedOrigins || []);
  const storageRoot = path.resolve(options.storageRoot || path.join(projectRoot, 'storage'));
  const maxUploadBytes = Math.min(100 * 1024 * 1024, Math.max(1024, Number(options.maxUploadBytes) || 25 * 1024 * 1024));
  const upload = multer({ storage: multer.memoryStorage(), limits: { files: 1, fileSize: maxUploadBytes, fields: 4 } });

  function limited(req, res, category, maximum, windowMs, identity = '') {
    const result = rate(`${category}:${clientIp(req)}:${identity}`, maximum, windowMs);
    if (result.allowed) return false;
    res.set('Retry-After', String(result.retryAfter)).status(429).json({ success: false, error: 'Too many requests' });
    return true;
  }
  function setSessionCookie(res, token, maxAge = SESSION_SECONDS) {
    const pieces = [`crisis_session=${encodeURIComponent(token)}`, 'Path=/', 'HttpOnly', 'SameSite=Strict', `Max-Age=${maxAge}`];
    if (secureCookie) pieces.push('Secure');
    res.append('Set-Cookie', pieces.join('; '));
  }
  function setCsrfCookie(res, token, maxAge = SESSION_SECONDS) {
    const pieces = [`crisis_csrf=${encodeURIComponent(token)}`, 'Path=/', 'SameSite=Strict', `Max-Age=${maxAge}`];
    if (secureCookie) pieces.push('Secure');
    res.append('Set-Cookie', pieces.join('; '));
  }
  function clearSessionCookie(res) { setSessionCookie(res, '', 0); }
  function clearAuthCookies(res) { clearSessionCookie(res); setCsrfCookie(res, '', 0); }
  function originAllowed(req) {
    const origin = req.get('origin');
    const sameOrigin = `${req.protocol}://${req.get('host')}`;
    return !origin || origin === sameOrigin || allowedOrigins.has(origin);
  }
  function sessionMiddleware(req, res, next) {
    const raw = cookieValue(req, 'crisis_session');
    if (!raw) return res.status(401).json({ success: false, error: 'Authentication required' });
    const db = store.read();
    const session = db.sessions[digest(raw, 'session:v1')];
    const user = session && db.users[session.user_id];
    if (!session || !user || session.expires_at <= nowUnix() || session.revoked_at || user.status !== 'active') {
      clearAuthCookies(res);
      return res.status(401).json({ success: false, error: 'Authentication required' });
    }
    req.account = { db, session, user, session_hash: digest(raw, 'session:v1') };
    next();
  }
  function csrfMiddleware(req, res, next) {
    if (!originAllowed(req)) return res.status(403).json({ success: false, error: 'Request origin is not allowed' });
    const supplied = req.get('x-csrf-token');
    if (!safeEqual(digest(String(supplied || ''), 'csrf:v1'), req.account.session.csrf_hash)) return res.status(403).json({ success: false, error: 'Invalid CSRF token' });
    next();
  }
  function requireRole(...roles) {
    return (req, res, next) => roles.includes(req.account.user.role) ? next() : res.status(403).json({ success: false, error: 'Insufficient permissions' });
  }
  function audit(db, req, actor, target, action, reason, previousValue, newValue) {
    db.audit_events.push({
      id: crypto.randomUUID(), actor_user_id: actor?.id || null, actor_role: actor?.role || 'system',
      target, action, reason: String(reason).trim().slice(0, 500), previous_value: previousValue,
      new_value: newValue, created_at: nowUnix(), request_id: requestId(req), metadata: metadata(req)
    });
  }
  function requireReason(req, res) {
    if (typeof req.body?.reason !== 'string' || req.body.reason.trim().length < 3 || req.body.reason.trim().length > 500) {
      res.status(400).json({ success: false, error: 'A reason between 3 and 500 characters is required' }); return null;
    }
    return req.body.reason.trim();
  }

  const auth = express.Router();
  auth.use(express.json({ limit: '16kb', strict: true }));
  auth.post('/register', async (req, res) => {
    const email = normalizeEmail(req.body?.email);
    if (limited(req, res, 'register', 5, 60 * 60 * 1000, email || 'invalid')) return;
    if (!email || !validatePassword(req.body?.password)) return res.status(400).json({ success: false, error: 'Use a valid email and a 12–128 character password containing letters and non-letters' });
    const passwordHash = await hashPassword(req.body.password);
    let created = false;
    store.transaction((db) => {
      if (Object.values(db.users).some((user) => user.email === email)) return;
      const id = crypto.randomUUID(); const now = nowUnix();
      db.users[id] = { id, admin_ref: randomToken(18), email, password_hash: passwordHash, role: 'user', status: 'active', email_verified: false, coin_balance: 0, subscription_expiry: null, failed_logins: 0, locked_until: 0, created_at: now, updated_at: now };
      created = true;
    });

    return res.status(202).json({ success: true, message: 'If the address can be registered, the account is ready to sign in.', email_verification_available: Boolean(options.sendVerification), created_for_testing: options.exposeTestState ? created : undefined });
  });
  auth.post('/login', async (req, res) => {
    const email = normalizeEmail(req.body?.email);
    if (limited(req, res, 'login', 10, 15 * 60 * 1000, email || 'invalid')) return;
    const db = store.read();
    const user = email ? Object.values(db.users).find((entry) => entry.email === email) : null;
    const valid = user && user.status === 'active' && user.locked_until <= nowUnix() && await verifyPassword(String(req.body?.password || ''), user.password_hash);
    if (!valid) {
      if (user) store.transaction((current) => { const target = current.users[user.id]; if (!target) return; target.failed_logins = (target.failed_logins || 0) + 1; if (target.failed_logins >= 10) target.locked_until = nowUnix() + Math.min(60, 2 ** Math.min(target.failed_logins - 8, 6)); });
      return res.status(401).json({ success: false, error: 'Invalid email or password' });
    }
    const token = randomToken(32); const csrf = randomToken(24); const sessionHash = digest(token, 'session:v1');
    store.transaction((current) => { const target = current.users[user.id]; target.failed_logins = 0; target.locked_until = 0; target.last_login_at = nowUnix(); current.sessions[sessionHash] = { user_id: user.id, csrf_hash: digest(csrf, 'csrf:v1'), created_at: nowUnix(), expires_at: nowUnix() + SESSION_SECONDS, metadata: metadata(req) }; });
    setSessionCookie(res, token);
    setCsrfCookie(res, csrf);
    res.json({ success: true, csrf_token: csrf, account: accountState(store.read(), user) });
  });
  auth.get('/session', sessionMiddleware, (req, res) => res.json({ success: true, account: accountState(req.account.db, req.account.user), csrf_token_required: true }));
  auth.post('/logout', sessionMiddleware, csrfMiddleware, (req, res) => {
    store.transaction((db) => { if (db.sessions[req.account.session_hash]) db.sessions[req.account.session_hash].revoked_at = nowUnix(); });
    clearAuthCookies(res); res.json({ success: true });
  });
  app.use('/api/auth', auth);

  const account = express.Router();
  account.use(express.json({ limit: '16kb', strict: true }), sessionMiddleware);
  account.get('/', (req, res) => res.json({ success: true, account: accountState(req.account.db, req.account.user) }));
  account.post('/password', csrfMiddleware, async (req, res) => {
    if (limited(req, res, 'password', 5, 60 * 60 * 1000, req.account.user.id)) return;
    if (!await verifyPassword(String(req.body?.current_password || ''), req.account.user.password_hash)) return res.status(400).json({ success: false, error: 'Password could not be changed' });
    if (!validatePassword(req.body?.new_password) || req.body.new_password === req.body.current_password) return res.status(400).json({ success: false, error: 'Choose a new 12–128 character password containing letters and non-letters' });
    const passwordHash = await hashPassword(req.body.new_password);
    store.transaction((db) => { db.users[req.account.user.id].password_hash = passwordHash; db.users[req.account.user.id].updated_at = nowUnix(); for (const session of Object.values(db.sessions)) if (session.user_id === req.account.user.id) session.revoked_at = nowUnix(); });
    clearAuthCookies(res); res.json({ success: true, message: 'Password changed. Sign in again.' });
  });
  account.post('/licenses/redeem', csrfMiddleware, (req, res) => {
    if (limited(req, res, 'license', 8, 60 * 60 * 1000, req.account.user.id)) return;
    const key = typeof req.body?.license_key === 'string' ? req.body.license_key.trim() : '';
    if (key.length < 4 || key.length > 256) return res.status(400).json({ success: false, error: 'License could not be redeemed' });
    let result = null;
    store.transaction((db) => {
      const license = db.keys[key]; const value = licenseDigest(key);
      if (!license || Object.values(db.account_license_links).some((link) => link.license_digest === value)) return;
      const id = crypto.randomUUID();
      db.account_license_links[id] = { id, user_id: req.account.user.id, license_digest: value, redeemed_at: nowUnix() };
      const user = db.users[req.account.user.id];
      if (Number.isSafeInteger(license.expiry_unix)) user.subscription_expiry = Math.max(user.subscription_expiry || 0, license.expiry_unix);
      if (license.discord_id && db.players[license.discord_id] && Number.isSafeInteger(db.players[license.discord_id].crisis_coins)) user.coin_balance = db.players[license.discord_id].crisis_coins;
      result = accountState(db, user);
    });
    if (!result) return res.status(400).json({ success: false, error: 'License could not be redeemed' });
    res.json({ success: true, account: result });
  });
  account.get('/devices', (req, res) => {
    const devices = Object.values(req.account.db.device_credentials).filter((entry) => entry.user_id === req.account.user.id).map(publicDevice);
    res.json({ success: true, devices });
  });
  account.post('/devices/revoke', csrfMiddleware, (req, res) => {
    let revoked = false;
    store.transaction((db) => { const device = Object.values(db.device_credentials).find((entry) => entry.public_id === req.body?.device_id && entry.user_id === req.account.user.id); if (device && !device.revoked_at) { device.revoked_at = nowUnix(); revoked = true; } });
    if (!revoked) return res.status(404).json({ success: false, error: 'Device not found' });
    res.json({ success: true });
  });
  account.post('/device/decision', csrfMiddleware, (req, res) => {
    if (limited(req, res, 'user-code', 10, 15 * 60 * 1000, req.account.user.id)) return;
    const codeHash = digest(String(req.body?.user_code || '').toUpperCase(), 'device-user-code:v1');
    const decision = req.body?.decision;
    if (!['approve', 'deny'].includes(decision)) return res.status(400).json({ success: false, error: 'Invalid decision' });
    let device = null;
    store.transaction((db) => {
      const authorization = Object.values(db.device_authorizations).find((entry) => entry.user_code_hash === codeHash && entry.status === 'pending' && entry.expires_at > nowUnix());
      if (!authorization) return;
      authorization.status = decision === 'approve' ? 'approved' : 'denied'; authorization.user_id = req.account.user.id; authorization.decided_at = nowUnix(); authorization.user_code_hash = null;
      device = { name: authorization.device_name, scopes: authorization.scopes, status: authorization.status };
    });
    if (!device) return res.status(400).json({ success: false, error: 'Code is invalid or expired' });
    res.json({ success: true, device });
  });
  account.post('/device/review', csrfMiddleware, (req, res) => {
    if (limited(req, res, 'user-code', 10, 15 * 60 * 1000, req.account.user.id)) return;
    const codeHash = digest(String(req.body?.user_code || '').toUpperCase(), 'device-user-code:v1');
    const authorization = Object.values(req.account.db.device_authorizations).find((entry) => entry.user_code_hash === codeHash && entry.status === 'pending' && entry.expires_at > nowUnix());
    if (!authorization) return res.status(400).json({ success: false, error: 'Code is invalid or expired' });
    res.json({ success: true, device: { name: authorization.device_name, scopes: authorization.scopes, expires_at: authorization.expires_at } });
  });
  app.use('/api/account', account);

  const device = express.Router();
  device.use(express.json({ limit: '8kb', strict: true }));
  device.post('/authorization', (req, res) => {
    if (limited(req, res, 'device-authorize', 20, 15 * 60 * 1000)) return;
    const name = typeof req.body?.device_name === 'string' ? req.body.device_name.trim().slice(0, 80) : '';
    const scopes = Array.isArray(req.body?.scopes) ? [...new Set(req.body.scopes)] : ['account:read'];
    if (!name || scopes.length < 1 || scopes.some((scope) => !DEVICE_SCOPES.has(scope))) return res.status(400).json({ success: false, error: 'Invalid device request' });
    const rawDeviceCode = randomToken(32); let rawUserCode; let stored;
    store.transaction((db) => {
      do { rawUserCode = userCode(); } while (Object.values(db.device_authorizations).some((entry) => entry.user_code_hash === digest(rawUserCode, 'device-user-code:v1')));
      const id = crypto.randomUUID(); stored = { id, device_code_hash: digest(rawDeviceCode, 'device-code:v1'), user_code_hash: digest(rawUserCode, 'device-user-code:v1'), device_name: name, scopes, status: 'pending', created_at: nowUnix(), expires_at: nowUnix() + DEVICE_CODE_SECONDS, interval: DEVICE_POLL_SECONDS, last_poll_at: 0 };
      db.device_authorizations[id] = stored;
    });
    res.status(201).json({ device_code: rawDeviceCode, user_code: rawUserCode, verification_uri: verificationUrl, expires_in: DEVICE_CODE_SECONDS, interval: DEVICE_POLL_SECONDS });
  });
  device.post('/token', (req, res) => {
    const raw = String(req.body?.device_code || '');
    if (limited(req, res, 'device-poll-ip', 120, 15 * 60 * 1000)) return;
    let response = { status: 'invalid_request' }; let httpStatus = 400;
    store.transaction((db) => {
      const entry = Object.values(db.device_authorizations).find((item) => safeEqual(item.device_code_hash, digest(raw, 'device-code:v1')));
      if (!entry || entry.status === 'consumed') return;
      const now = nowUnix();
      if (entry.expires_at <= now) { entry.status = 'expired'; response = { status: 'expired_token' }; return; }
      if (entry.last_poll_at && now - entry.last_poll_at < entry.interval) { entry.interval = Math.min(30, entry.interval + 1); response = { status: 'slow_down', interval: entry.interval }; httpStatus = 429; return; }
      entry.last_poll_at = now;
      if (entry.status === 'pending') { response = { status: 'authorization_pending' }; httpStatus = 202; return; }
      if (entry.status === 'denied') { response = { status: 'access_denied' }; return; }
      if (entry.status !== 'approved' || !entry.user_id) return;
      const credential = randomToken(32); const credentialHash = digest(credential, 'device-credential:v1');
      db.device_credentials[credentialHash] = { public_id: randomToken(18), user_id: entry.user_id, name: entry.device_name, scopes: entry.scopes, created_at: now, last_used_at: null, revoked_at: null };
      entry.status = 'consumed'; entry.device_code_hash = null;
      response = { status: 'authorized', device_credential: credential, token_type: 'Device', scopes: entry.scopes }; httpStatus = 200;
    });
    res.status(httpStatus).json(response);
  });
  function deviceCredential(req, res, next) {
    const authorization = String(req.get('authorization') || '');
    const raw = authorization.startsWith('Device ') ? authorization.slice(7) : '';
    const db = store.read(); const credentialHash = digest(raw, 'device-credential:v1'); const credential = db.device_credentials[credentialHash];
    const user = credential && db.users[credential.user_id];
    if (!credential || credential.revoked_at || !user || user.status !== 'active') return res.status(401).json({ success: false, error: 'Unauthorized' });
    req.deviceContext = { credentialHash, credential, user, db }; next();
  }
  device.get('/account', deviceCredential, (req, res) => {
    if (!req.deviceContext.credential.scopes.includes('account:read')) return res.status(403).json({ success: false, error: 'Insufficient scope' });
    store.transaction((db) => { if (db.device_credentials[req.deviceContext.credentialHash]) db.device_credentials[req.deviceContext.credentialHash].last_used_at = nowUnix(); });
    res.json({ success: true, account: accountState(req.deviceContext.db, req.deviceContext.user) });
  });
  device.post('/rotate', deviceCredential, (req, res) => {
    if (limited(req, res, 'device-rotate', 5, 60 * 60 * 1000, req.deviceContext.credential.public_id)) return;
    const raw = randomToken(32); const nextHash = digest(raw, 'device-credential:v1');
    store.transaction((db) => { const previous = db.device_credentials[req.deviceContext.credentialHash]; if (!previous || previous.revoked_at) throw new Error('Credential already revoked'); previous.revoked_at = nowUnix(); db.device_credentials[nextHash] = { ...previous, public_id: randomToken(18), created_at: nowUnix(), last_used_at: null, revoked_at: null, rotated_from: previous.public_id }; });
    res.json({ success: true, device_credential: raw, token_type: 'Device', scopes: req.deviceContext.credential.scopes });
  });
  app.use('/api/device', device);

  const admin = express.Router();
  admin.use(express.json({ limit: '32kb', strict: true }), sessionMiddleware, requireRole('admin', 'support'));
  admin.use((req, res, next) => {
    if (req.method === 'GET' || !limited(req, res, 'admin-operation', 60, 60 * 1000, req.account.user.id)) next();
  });
  admin.get('/users', (req, res) => {
    const page = Math.max(1, Number.parseInt(req.query.page, 10) || 1); const pageSize = Math.min(100, Math.max(1, Number.parseInt(req.query.page_size, 10) || 20));
    const query = String(req.query.q || '').toLowerCase().slice(0, 128); const status = String(req.query.status || '');
    let users = Object.values(req.account.db.users).filter((user) => (!query || user.email.includes(query)) && (!status || user.status === status));
    users.sort((a, b) => b.created_at - a.created_at); const total = users.length;
    users = users.slice((page - 1) * pageSize, page * pageSize).map((user) => ({ admin_ref: user.admin_ref, role: user.role, ...accountState(req.account.db, user) }));
    res.json({ success: true, users, pagination: { page, page_size: pageSize, total, pages: Math.ceil(total / pageSize) } });
  });
  admin.get('/users/:ref', (req, res) => {
    const user = Object.values(req.account.db.users).find((entry) => entry.admin_ref === req.params.ref);
    if (!user) return res.status(404).json({ success: false, error: 'User not found' });
    const devices = Object.values(req.account.db.device_credentials).filter((entry) => entry.user_id === user.id).map(publicDevice);
    const history = req.account.db.audit_events.filter((entry) => entry.target === `user:${user.admin_ref}`).slice(-100).reverse().map((entry) => ({
      action: entry.action, reason: entry.reason, previous_value: entry.previous_value, new_value: entry.new_value,
      created_at: entry.created_at, request_id: entry.request_id, metadata: entry.metadata,
      actor: req.account.db.users[entry.actor_user_id]?.email || entry.actor_role
    }));
    res.json({ success: true, user: { admin_ref: user.admin_ref, role: user.role, ...accountState(req.account.db, user) }, devices, audit_history: history });
  });
  admin.post('/users/:ref/coins', requireRole('admin'), csrfMiddleware, (req, res) => {
    const reason = requireReason(req, res); const delta = req.body?.delta; if (!reason) return;
    if (!Number.isSafeInteger(delta) || delta === 0 || Math.abs(delta) > 1000000000) return res.status(400).json({ success: false, error: 'Invalid coin adjustment' });
    let next = null;
    store.transaction((db) => { const user = Object.values(db.users).find((entry) => entry.admin_ref === req.params.ref); if (!user) return; const previous = user.coin_balance || 0; if (!Number.isSafeInteger(previous + delta) || previous + delta < 0) return; next = previous + delta; user.coin_balance = next; for (const [, license] of linkedLicenses(db, user.id)) if (license.discord_id && db.players[license.discord_id]) db.players[license.discord_id].crisis_coins = next; db.coin_ledger.push({ id: crypto.randomUUID(), user_id: user.id, delta, previous, balance: next, reason, actor_user_id: req.account.user.id, created_at: nowUnix() }); audit(db, req, req.account.user, `user:${user.admin_ref}`, 'coins.adjust', reason, previous, next); });
    if (next === null) return res.status(409).json({ success: false, error: 'Adjustment could not be applied' }); res.json({ success: true, coin_balance: next });
  });
  admin.post('/users/:ref/subscription', requireRole('admin'), csrfMiddleware, (req, res) => {
    const reason = requireReason(req, res); const seconds = req.body?.seconds; if (!reason) return;
    if (!Number.isSafeInteger(seconds) || seconds === 0 || Math.abs(seconds) > 10 * 365 * 86400) return res.status(400).json({ success: false, error: 'Invalid subscription adjustment' });
    let next = null;
    store.transaction((db) => { const user = Object.values(db.users).find((entry) => entry.admin_ref === req.params.ref); if (!user) return; const now = nowUnix(); const previous = user.subscription_expiry || now; const candidate = previous + seconds; if (!Number.isSafeInteger(candidate) || candidate < now) return; next = candidate; user.subscription_expiry = next; for (const [, license] of linkedLicenses(db, user.id)) license.expiry_unix = next; db.subscription_ledger.push({ id: crypto.randomUUID(), user_id: user.id, seconds, previous, expiry: next, reason, actor_user_id: req.account.user.id, created_at: now }); audit(db, req, req.account.user, `user:${user.admin_ref}`, 'subscription.adjust', reason, previous, next); });
    if (next === null) return res.status(409).json({ success: false, error: 'Adjustment could not be applied' }); res.json({ success: true, subscription_expiry: next });
  });
  admin.post('/users/:ref/status', requireRole('admin'), csrfMiddleware, (req, res) => {
    const reason = requireReason(req, res); const status = req.body?.status; if (!reason) return;
    if (!['active', 'suspended'].includes(status)) return res.status(400).json({ success: false, error: 'Invalid status' });
    let changed = false;
    store.transaction((db) => { const user = Object.values(db.users).find((entry) => entry.admin_ref === req.params.ref); if (!user || user.id === req.account.user.id) return; if (user.role === 'admin' && status === 'suspended' && Object.values(db.users).filter((entry) => entry.role === 'admin' && entry.status === 'active').length <= 1) return; const previous = user.status; user.status = status; if (status === 'suspended') { for (const session of Object.values(db.sessions)) if (session.user_id === user.id) session.revoked_at = nowUnix(); for (const credential of Object.values(db.device_credentials)) if (credential.user_id === user.id) credential.revoked_at = nowUnix(); } audit(db, req, req.account.user, `user:${user.admin_ref}`, `account.${status}`, reason, previous, status); changed = true; });
    if (!changed) return res.status(409).json({ success: false, error: 'Status could not be changed' }); res.json({ success: true, status });
  });
  admin.post('/users/:ref/hwid-reset', requireRole('admin'), csrfMiddleware, (req, res) => {
    const reason = requireReason(req, res); if (!reason) return; let count = 0;
    store.transaction((db) => { const user = Object.values(db.users).find((entry) => entry.admin_ref === req.params.ref); if (!user) return; for (const [, license] of linkedLicenses(db, user.id)) { if (license.hwid || license.client_public_key) { license.hwid = null; license.client_public_key = null; count += 1; } } audit(db, req, req.account.user, `user:${user.admin_ref}`, 'hwid.reset', reason, { associations: count }, { associations: 0 }); });
    if (!count) return res.status(409).json({ success: false, error: 'No HWID association was reset' }); res.json({ success: true, reset_count: count });
  });
  admin.get('/files', requireRole('admin'), (req, res) => {
    res.json({ success: true, slots: Object.entries(FILE_SLOTS).map(([slot, config]) => ({ slot, filename: config.filename, metadata: req.account.db.file_slots[slot] || null })) });
  });
  admin.post('/files/:slot', requireRole('admin'), csrfMiddleware, upload.single('file'), (req, res) => {
    const reason = requireReason(req, res); const config = FILE_SLOTS[req.params.slot];
    if (!reason) return;
    if (!config || !req.file) return res.status(400).json({ success: false, error: 'A valid file slot and file are required' });
    const originalExtension = path.extname(req.file.originalname || '').toLowerCase();
    const allowedMime = new Set(['application/octet-stream', 'application/x-msdownload', 'application/vnd.microsoft.portable-executable']);
    const buffer = req.file.buffer;
    let validPe = buffer.length >= 64 && buffer[0] === 0x4d && buffer[1] === 0x5a;
    if (validPe) { const peOffset = buffer.readUInt32LE(0x3c); validPe = peOffset >= 64 && peOffset + 4 <= buffer.length && buffer.readUInt32LE(peOffset) === 0x00004550; }
    if (originalExtension !== config.extension || !allowedMime.has(req.file.mimetype) || !validPe) return res.status(415).json({ success: false, error: 'File type or content is not approved for this slot' });
    const currentDirectory = path.join(storageRoot, 'current'); const versionDirectory = path.join(storageRoot, 'versions', req.params.slot);
    const destination = path.join(currentDirectory, config.filename); const temporary = path.join(currentDirectory, `.${crypto.randomUUID()}.upload`);
    let metadataRecord; let backupPath = null; const existed = fs.existsSync(destination);
    try {
      fs.mkdirSync(currentDirectory, { recursive: true, mode: 0o700 }); fs.mkdirSync(versionDirectory, { recursive: true, mode: 0o700 });
      if (existed) {
        const versionName = `${Date.now()}-${crypto.randomUUID()}${config.extension}`;
        backupPath = path.join(versionDirectory, versionName);
        fs.copyFileSync(destination, backupPath, fs.constants.COPYFILE_EXCL);
      }
      fs.writeFileSync(temporary, buffer, { mode: 0o600, flag: 'wx' }); fs.renameSync(temporary, destination);
      metadataRecord = { filename: config.filename, size: buffer.length, sha256: crypto.createHash('sha256').update(buffer).digest('hex'), mime_type: req.file.mimetype, uploaded_at: nowUnix(), uploaded_by: req.account.user.admin_ref };
      store.transaction((db) => { const previous = db.file_slots[req.params.slot] || null; db.file_slots[req.params.slot] = metadataRecord; audit(db, req, req.account.user, `file:${req.params.slot}`, 'file.replace', reason, previous, metadataRecord); });
      res.json({ success: true, slot: req.params.slot, metadata: metadataRecord });
    } catch (error) {
      try { fs.unlinkSync(temporary); } catch {}
      try {
        if (backupPath) { const restore = path.join(currentDirectory, `.${crypto.randomUUID()}.restore`); fs.copyFileSync(backupPath, restore, fs.constants.COPYFILE_EXCL); fs.renameSync(restore, destination); }
        else if (!existed && fs.existsSync(destination)) fs.unlinkSync(destination);
      } catch (rollbackError) { console.error(`[UPLOAD ${req.id}] Rollback failed:`, rollbackError.message); }
      console.error(`[UPLOAD ${req.id}] Replacement failed:`, error.message);
      res.status(500).json({ success: false, error: 'File replacement failed' });
    }
  });
  app.use('/api/admin', admin);

  app.use(express.static(path.join(projectRoot, 'public'), { index: false, dotfiles: 'deny', etag: true, maxAge: '1h' }));
  app.get(['/account', '/device'], (req, res) => res.sendFile(path.join(projectRoot, 'public', req.path === '/device' ? 'device.html' : 'account.html')));
  app.get('/admin', sessionMiddleware, requireRole('admin', 'support'), (req, res) => res.sendFile(path.join(projectRoot, 'public', 'admin.html')));

  app.use((error, req, res, next) => {
    if (res.headersSent) return next(error);
    const parserError = error?.type === 'entity.parse.failed' || error?.type === 'entity.too.large';
    const status = error instanceof multer.MulterError ? (error.code === 'LIMIT_FILE_SIZE' ? 413 : 400) : parserError ? (error.type === 'entity.too.large' ? 413 : 400) : 500;
    console.error(`[REQUEST ${req.id || 'unknown'}]`, error.message);
    res.status(status).json({ success: false, error: status === 413 ? 'Uploaded file is too large' : status === 400 ? 'Invalid upload' : 'Request failed' });
  });

  return { csrfMiddleware, requireRole, sessionMiddleware, audit, requireReason };
}

module.exports = { DEVICE_CODE_SECONDS, DEVICE_POLL_SECONDS, SESSION_SECONDS, accountState, licenseDigest, linkedLicenses, mountAccountService };
