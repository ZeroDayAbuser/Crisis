const express = require('express');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { createStore } = require('./lib/data-store');
const { mountAccountService } = require('./account-service');
const { createRateLimiter, digest, metadata, randomToken, requestId } = require('./lib/security');
const { isLicensePaused, pauseLicense } = require('./lib/license-state');
const { licenseAllowsGame, parseGameSelection, requiredGameForModule } = require('./lib/game-entitlements');
const { createModuleFileManager } = require('./lib/module-files');
const { createClientProofVerifier } = require('./lib/client-proof');
const { mountDiscordMemberRoute } = require('./discord/member-route');
const { issuePairingCode, mountDiscordBot, registerDiscordCommands, startDiscordPresence } = require('./discord/bot');
const { bindPlayerToLicense, mountPlayerRoutes } = require('./player-service');
const { createNativeServer } = require('./native-server');

const app = express();
app.disable('x-powered-by');
if (process.env.NODE_ENV === 'production') app.set('trust proxy', String(process.env.CRISIS_TRUST_PROXY || 'loopback'));
app.use((req, res, next) => {
  if (process.env.NODE_ENV === 'production' && !req.secure) {
    return res.status(426).json({ success: false, error: 'HTTPS is required' });
  }
  req.id = crypto.randomUUID();
  res.set('X-Request-ID', req.id);
  res.set('Cache-Control', 'no-store');
  res.set('X-Content-Type-Options', 'nosniff');
  res.set('Referrer-Policy', 'no-referrer');
  res.set('X-Frame-Options', 'DENY');
  res.set('Permissions-Policy', 'camera=(), microphone=(), geolocation=()');
  res.set('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' https: data:; connect-src 'self'; object-src 'none'; base-uri 'none'; form-action 'self'; frame-ancestors 'none'");
  if (process.env.NODE_ENV === 'production') res.set('Strict-Transport-Security', 'max-age=31536000; includeSubDomains');
  const origin = req.get('origin');
  const allowed = String(process.env.CRISIS_CORS_ORIGINS || '').split(',').map((value) => value.trim()).filter(Boolean);
  if (origin && allowed.includes(origin)) { res.set('Access-Control-Allow-Origin', origin); res.set('Vary', 'Origin'); res.set('Access-Control-Allow-Credentials', 'true'); }
  if (req.method === 'OPTIONS') {
    if (!origin || !allowed.includes(origin)) return res.sendStatus(403);
    res.set('Access-Control-Allow-Methods', 'GET,POST,PATCH,DELETE');
    res.set('Access-Control-Allow-Headers', 'Content-Type,X-CSRF-Token');
    return res.sendStatus(204);
  }
  next();
});
const PORT = process.env.PORT || 1053;
const NATIVE_PORT = process.env.CRISIS_NATIVE_PORT || 223;
const PROJECT_ROOT = path.join(__dirname, '..');
const production = process.env.NODE_ENV === 'production';
if (production && !process.env.DATABASE_PATH) {
  throw new Error('DATABASE_PATH is required in production and must point to persistent storage outside the application checkout.');
}
const DB_PATH = process.env.DATABASE_PATH ? path.resolve(process.env.DATABASE_PATH) : path.join(PROJECT_ROOT, 'database.json');
const store = createStore(DB_PATH, { allowCreate: !production });
store.initialize();
const moduleFiles = createModuleFileManager({ storageRoot: process.env.CRISIS_UPLOAD_ROOT, maxUploadBytes: process.env.CRISIS_UPLOAD_MAX_BYTES });
const legacyAdminRate = createRateLimiter();
const nativeAuthRate = createRateLimiter();
const clientProofs = createClientProofVerifier();

const CLIENT_ACCESS_SECONDS = 5 * 60;
const CLIENT_ACCESS_LIMIT = 10000;
const CLIENT_ACCESS_TOKEN = /^[A-Za-z0-9_-]{43}$/;
const clientAccessSessions = new Map();
const clientAccessByLicense = new Map();
const playerAccessSessions = new Map();
const playerAccessByLicense = new Map();

function loadDatabase() {
  return store.read();
}

function saveDatabase(db) {
  if (!db || typeof db !== 'object' || Array.isArray(db) || !db.keys || typeof db.keys !== 'object') {
    console.error('[DB] Refusing to write incomplete database snapshot');
    return false;
  }
  return store.write(db);
}

function secretMatches(expected, supplied) {
  if (typeof expected !== 'string' || expected.length < 32 || typeof supplied !== 'string') return false;
  const expectedHash = crypto.createHash('sha256').update(expected).digest();
  const suppliedHash = crypto.createHash('sha256').update(supplied).digest();
  return crypto.timingSafeEqual(expectedHash, suppliedHash);
}

function keyLabel(key) {
  return typeof key === 'string' && key.length > 4 ? `${key.slice(0, 4)}...` : '[redacted]';
}
function removeAccessSession(sessions, sessionsByLicense, tokenHash, session) {
  sessions.delete(tokenHash);
  if (sessionsByLicense.get(session.licenseId) === tokenHash) sessionsByLicense.delete(session.licenseId);
}

function pruneAccessSessions(sessions, sessionsByLicense, now = Math.floor(Date.now() / 1000)) {
  for (const [tokenHash, session] of sessions) {
    if (session.expiresAt <= now) removeAccessSession(sessions, sessionsByLicense, tokenHash, session);
  }
}

function replaceAccessSession(sessions, sessionsByLicense, { key, hwid, clientPublicKey, licenseId, expiresAt, digestDomain }) {
  const previousHash = sessionsByLicense.get(licenseId);
  const previous = previousHash && sessions.get(previousHash);
  if (previous) removeAccessSession(sessions, sessionsByLicense, previousHash, previous);
  if (sessions.size >= CLIENT_ACCESS_LIMIT) throw new Error('Client access session capacity reached');

  const token = randomToken(32);
  const tokenHash = digest(token, digestDomain);
  sessions.set(tokenHash, { key, hwid, clientPublicKey, licenseId, expiresAt });
  sessionsByLicense.set(licenseId, tokenHash);
  return token;
}

function issueClientAccess(key, hwid, clientPublicKey, licenseExpiry) {
  const now = Math.floor(Date.now() / 1000);
  pruneAccessSessions(clientAccessSessions, clientAccessByLicense, now);
  pruneAccessSessions(playerAccessSessions, playerAccessByLicense, now);

  const licenseId = digest(key, 'client-access-license:v1');
  const accessExpiresAt = Math.min(licenseExpiry, now + CLIENT_ACCESS_SECONDS);
  const accessToken = replaceAccessSession(clientAccessSessions, clientAccessByLicense, {
    key, hwid, clientPublicKey, licenseId, expiresAt: accessExpiresAt, digestDomain: 'client-access-token:v1'
  });
  const playerToken = replaceAccessSession(playerAccessSessions, playerAccessByLicense, {
    key, hwid, clientPublicKey, licenseId, expiresAt: licenseExpiry, digestDomain: 'player-access-token:v1'
  });
  return {
    access_token: accessToken,
    token_type: 'Bearer',
    access_token_expires_unix: accessExpiresAt,
    access_token_expires_in: Math.max(0, accessExpiresAt - now),
    player_token: playerToken,
    player_token_type: 'Bearer',
    player_token_expires_unix: licenseExpiry
  };
}

function authenticateSession(req, database, sessions, sessionsByLicense, digestDomain) {
  const authorization = String(req.get('authorization') || '');
  const token = authorization.startsWith('Bearer ') ? authorization.slice(7) : '';
  if (!CLIENT_ACCESS_TOKEN.test(token)) return null;

  const tokenHash = digest(token, digestDomain);
  const session = sessions.get(tokenHash);
  if (!session) return null;

  const now = Math.floor(Date.now() / 1000);
  const license = database.keys?.[session.key];
  if (session.expiresAt <= now || !license || isLicensePaused(database, license) ||
      license.hwid !== session.hwid || !Number.isSafeInteger(license.expiry_unix) || license.expiry_unix <= now) {
    removeAccessSession(sessions, sessionsByLicense, tokenHash, session);
    return null;
  }
  return { accessToken: token, key: session.key, hwid: session.hwid, license };
}

function authenticatePlayerAccess(req, database = loadDatabase()) {
  return authenticateSession(req, database, playerAccessSessions, playerAccessByLicense, 'player-access-token:v1');
}

function authenticateAccessToken(token, proof, database = loadDatabase()) {
  if (!CLIENT_ACCESS_TOKEN.test(token)) return null;
  const tokenHash = digest(token, 'client-access-token:v1');
  const session = clientAccessSessions.get(tokenHash);
  if (!session) return null;

  const now = Math.floor(Date.now() / 1000);
  const license = database.keys?.[session.key];
  if (session.expiresAt <= now || !license || isLicensePaused(database, license) ||
      license.hwid !== session.hwid || !Number.isSafeInteger(license.expiry_unix) || license.expiry_unix <= now) {
    removeAccessSession(clientAccessSessions, clientAccessByLicense, tokenHash, session);
    return null;
  }
  if (!clientProofs.verifyArtifact(proof, token, session.clientPublicKey)) return null;
  return { accessToken: token, key: session.key, hwid: session.hwid, license };
}


function nativeError(status, error, extra = {}) {
  return { status, body: { success: false, error, ...extra } };
}

function authenticateNative(body, ip) {
  const request = body && typeof body === 'object' && !Array.isArray(body) ? body : {};
  const { key, hwid } = request;

  const rate = nativeAuthRate(`native-auth:${ip || 'unknown'}`, 30, 60 * 1000);
  if (!rate.allowed) {
    return nativeError(429, 'Too many authentication attempts', { retry_after: rate.retryAfter });
  }

  if (typeof key !== 'string' || key.length < 8 || key.length > 256) {
    return nativeError(400, 'Missing or invalid key');
  }
  if (typeof hwid !== 'string' || !/^[\x21-\x7e]{1,256}$/.test(hwid)) {
    return nativeError(400, 'Missing or invalid hwid');
  }
  const clientProof = clientProofs.verifyAuthentication(request);
  if (!clientProof) return nativeError(401, 'Client device proof required');
  const clientProfileFields = ['discord_id', 'username', 'display_name', 'avatar_url'];
  if (clientProfileFields.some((field) => Object.prototype.hasOwnProperty.call(request, field))) {
    return nativeError(400, 'Discord identity must be linked with the server-issued pairing code');
  }

  try {
    const existing = store.read();
    if (!existing.keys || !existing.keys[key]) {
      console.warn(`[AUTH] Key not found: key=${keyLabel(key)}, database=${DB_PATH}`);
      return nativeError(200, 'Key not found');
    }

    const existingKey = existing.keys[key];
    const now = Math.floor(Date.now() / 1000);
    if (existingKey.client_public_key && existingKey.client_public_key !== clientProof.encodedPublicKey) {
      return nativeError(401, 'Client device proof does not match this license');
    }
    if (isLicensePaused(existing, existingKey)) {
      return nativeError(423, 'Subscription is paused');
    }

    const needsActivation = existingKey.start_unix === null || existingKey.start_unix === undefined;
    if (!needsActivation) {
      if (existingKey.hwid && existingKey.hwid !== hwid) {
        console.log(`[AUTH] HWID mismatch: key=${keyLabel(key)}`);
        return nativeError(200, 'HWID mismatch');
      }
      if (!Number.isSafeInteger(existingKey.expiry_unix) || now >= existingKey.expiry_unix) {
        console.log(`[AUTH] Key expired: key=${keyLabel(key)}, expiry=${existingKey.expiry_unix}, now=${now}`);
        return nativeError(200, 'Key expired', { expiry_unix: existingKey.expiry_unix });
      }
    }

    const needsPersist = needsActivation || !existingKey.discord_id || !existingKey.client_public_key || !existingKey.hwid;
    if (!needsPersist) {
      const remaining = existingKey.expiry_unix - now;
      console.log(`[AUTH] Valid login: key=${keyLabel(key)}, remaining=${remaining}s`);
      return {
        status: 200,
        body: {
          success: true,
          games: parseGameSelection(existingKey.games) || [],
          start_unix: existingKey.start_unix,
          expiry_unix: existingKey.expiry_unix,
          duration_seconds: existingKey.expiry_unix - existingKey.start_unix,
          remaining_seconds: remaining,
          ...issueClientAccess(key, hwid, clientProof.encodedPublicKey, existingKey.expiry_unix)
        }
      };
    }

    let status = 200;
    let responseBody;
    store.transaction((db) => {
      const keyData = db.keys?.[key];
      if (!keyData) {
        console.warn(`[AUTH] Key disappeared during authentication transaction: key=${keyLabel(key)}, database=${DB_PATH}`);
        responseBody = { success: false, error: 'Key not found' };
        return;
      }
      if (isLicensePaused(db, keyData)) {
        status = 423;
        responseBody = { success: false, error: 'Subscription is paused' };
        return;
      }

      if (keyData.start_unix === null || keyData.start_unix === undefined) {
        const duration = keyData.duration_seconds || 2592000;
        keyData.start_unix = now;
        keyData.expiry_unix = now + duration;
        keyData.hwid = hwid;
        keyData.client_public_key = clientProof.encodedPublicKey;
        const pairing = !keyData.discord_id ? issuePairingCode(db, key) : null;
        console.log(`[AUTH] First login: key=${keyLabel(key)}, start=${keyData.start_unix}, expiry=${keyData.expiry_unix}`);
        responseBody = {
          success: true,
          games: parseGameSelection(keyData.games) || [],
          start_unix: keyData.start_unix,
          expiry_unix: keyData.expiry_unix,
          duration_seconds: duration,
          message: 'Key activated successfully',
          ...(pairing ? { pairing_code: pairing.code, pairing_expires_in: pairing.expires_in } : {})
        };
        return;
      }
      if (keyData.client_public_key && keyData.client_public_key !== clientProof.encodedPublicKey) {
        status = 401;
        responseBody = { success: false, error: 'Client device proof does not match this license' };
        return;
      }
      keyData.client_public_key = clientProof.encodedPublicKey;

      if (keyData.hwid && keyData.hwid !== hwid) {
        responseBody = { success: false, error: 'HWID mismatch' };
        return;
      }
      keyData.hwid = hwid;
      if (!Number.isSafeInteger(keyData.expiry_unix) || now >= keyData.expiry_unix) {
        responseBody = { success: false, error: 'Key expired', expiry_unix: keyData.expiry_unix };
        return;
      }

      const remaining = keyData.expiry_unix - now;
      const duration = keyData.expiry_unix - keyData.start_unix;
      console.log(`[AUTH] Valid login: key=${keyLabel(key)}, remaining=${remaining}s`);
      const pairing = !keyData.discord_id ? issuePairingCode(db, key) : null;
      responseBody = {
        success: true,
        games: parseGameSelection(keyData.games) || [],
        start_unix: keyData.start_unix,
        expiry_unix: keyData.expiry_unix,
        duration_seconds: duration,
        remaining_seconds: remaining,
        ...(pairing ? { pairing_code: pairing.code, pairing_expires_in: pairing.expires_in } : {})
      };
    });
    if (responseBody?.success) Object.assign(responseBody, issueClientAccess(key, hwid, clientProof.encodedPublicKey, responseBody.expiry_unix));
    return { status, body: responseBody };
  } catch (error) {
    console.error('[AUTH] Authentication failed:', error.message);
    return nativeError(500, 'Authentication failed');
  }
}

function goneNative(req, res) {
  return res.status(410).json({ success: false, error: 'Native authentication uses TCP sockets, not HTTP' });
}

app.get('/', (req, res) => {
  res.set('Allow', 'GET,POST');
  return res.status(405).json({ success: false, error: 'Native authentication uses TCP sockets, not HTTP' });
});
app.post('/api/client/authenticate', goneNative);
app.get('/module', goneNative);
app.get('/driver', goneNative);

mountDiscordMemberRoute(app, { loadDatabase });
mountDiscordBot(app, { store, bindPlayerToLicense, moduleFiles });
const playerRouter = mountPlayerRoutes(app, {
  store,
  loadDatabase,
  saveDatabase,
  eventSecret: process.env.CRISIS_EVENT_SECRET,
  authenticateAccess: authenticatePlayerAccess,
  casinoRateLimit: {
    windowMs: Number(process.env.CRISIS_CASINO_RATE_WINDOW_MS) || 10000,
    max: Number(process.env.CRISIS_CASINO_RATE_MAX) || 30
  }
});

const PLAYER_NATIVE_OPS = Object.freeze({
  player: ['GET', '/'],
  'player.scoreboard': ['GET', '/scoreboard'],
  'player.gameplay.match.start': ['POST', '/gameplay/match/start'],
  'player.gameplay.elimination': ['POST', '/gameplay/elimination'],
  'player.gameplay.match.end': ['POST', '/gameplay/match/end'],
  'player.coins.purchase': ['POST', '/coins/purchase'],
  'player.subscription.redeem': ['POST', '/subscription/redeem'],
  'player.subscription.redeem-xp': ['POST', '/subscription/redeem-xp'],
  'casino.state': ['GET', '/casino/state'],
  'casino.roulette': ['POST', '/casino/roulette'],
  'casino.crash': ['POST', '/casino/crash'],
  'casino.crate': ['POST', '/casino/crate'],
  'casino.mines.start': ['POST', '/casino/mines/start'],
  'casino.mines.reveal': ['POST', '/casino/mines/reveal'],
  'casino.mines.cashout': ['POST', '/casino/mines/cashout'],
  'casino.chicken.start': ['POST', '/casino/chicken/start'],
  'casino.chicken.jump': ['POST', '/casino/chicken/jump'],
  'casino.chicken.cashout': ['POST', '/casino/chicken/cashout'],
  'casino.blackjack.start': ['POST', '/casino/blackjack/start'],
  'casino.blackjack.action': ['POST', '/casino/blackjack/action']
});

function dispatchPlayerNative(json) {
  const mapped = PLAYER_NATIVE_OPS[String(json?.op || '')];
  if (!mapped) return nativeError(400, 'Unknown native operation');
  const token = typeof json.player_token === 'string' ? json.player_token : (typeof json.access_token === 'string' ? json.access_token : '');
  const body = { ...json };
  delete body.op;
  delete body.player_token;
  delete body.access_token;
  const [method, urlPath] = mapped;
  return new Promise((resolve) => {
    const req = {
      method,
      url: urlPath,
      originalUrl: urlPath,
      body,
      headers: { authorization: `Bearer ${token}` },
      get(name) { return this.headers[String(name || '').toLowerCase()] || ''; }
    };
    const res = {
      statusCode: 200,
      status(code) { this.statusCode = Number(code) || 200; return this; },
      set() { return this; },
      json(payload) { resolve({ status: this.statusCode || 200, body: payload }); return this; }
    };
    playerRouter.handle(req, res, (error) => {
      if (error) resolve({ status: error.status || 500, body: { success: false, error: error.message || 'Player request failed' } });
      else resolve({ status: 404, body: { success: false, error: 'Unknown player operation' } });
    });
  });
}

app.use('/admin/create-key', express.json({ limit: '8kb', strict: true }), (req, res, next) => {
  const configured = process.env.CRISIS_ADMIN_SECRET;
  const bearer = req.get('authorization') || '';
  const supplied = req.get('x-crisis-admin-secret') || (bearer.startsWith('Bearer ') ? bearer.slice(7) : '');
  if (typeof configured !== 'string' || configured.length < 32)
    return res.status(503).json({ success: false, error: 'Admin API is disabled until CRISIS_ADMIN_SECRET is configured' });
  if (!secretMatches(configured, supplied))
    return res.status(403).json({ success: false, error: 'Admin authorization required' });
  next();
});
app.post('/admin/create-key', (req, res) => {
  const { key, duration_seconds, games: gameSelection, reason } = req.body;
  const rate = legacyAdminRate(`legacy-admin:${req.ip || 'unknown'}`, 20, 60 * 1000);
  if (!rate.allowed) { res.set('Retry-After', String(rate.retryAfter)); return res.status(429).json({ success: false, error: 'Too many requests' }); }
  const games = parseGameSelection(gameSelection);
  if (typeof key !== 'string' || key.length < 8 || key.length > 256 || !Number.isSafeInteger(duration_seconds) || duration_seconds < 60 || duration_seconds > 10 * 365 * 86400 || !games || typeof reason !== 'string' || reason.trim().length < 3 || reason.trim().length > 500) {
    return res.status(400).json({ success: false, error: 'A valid key, duration_seconds, games, and reason are required' });
  }
  let created = false;
  store.transaction((db) => {
    if (db.keys[key]) return;
    db.keys[key] = { duration_seconds, games: [...games], start_unix: null, expiry_unix: null, hwid: null, created_at: Math.floor(Date.now() / 1000), pause_reasons: [], pause_started_at: null };
    if (db.service_state?.keys_paused) pauseLicense(db.keys[key], 'global');
    db.audit_events.push({ id: crypto.randomUUID(), actor_user_id: null, actor_role: 'license_issuer_service', target: `license:${crypto.createHash('sha256').update(key).digest('base64url')}`, action: 'license.create', reason: reason.trim(), previous_value: null, new_value: { duration_seconds, games: [...games] }, created_at: Math.floor(Date.now() / 1000), request_id: requestId(req), metadata: metadata(req) });
    created = true;
  });
  if (!created) return res.status(409).json({ success: false, error: 'Key already exists' });

  console.log(`[ADMIN] Created key: ${keyLabel(key)}, duration: ${duration_seconds}s`);
  return res.json({ success: true, message: 'Key created', key, games });
});


function deriveXorKey(key, hwid) {
  const seed = key + hwid;
  let hash = 0;
  for (let i = 0; i < seed.length; i++) {
    hash = ((hash << 5) - hash) + seed.charCodeAt(i);
    hash |= 0;
  }
  const xorBytes = Buffer.alloc(256);
  for (let i = 0; i < 256; i++) {
    xorBytes[i] = (hash ^ (i * 0x9E)) & 0xFF;
    hash = ((Math.imul(hash, 1103515245) + 12345) >>> 0);
  }
  return xorBytes;
}

function xorBuffer(buffer, xorKey) {
  for (let i = 0; i < buffer.length; i++) {
    buffer[i] ^= xorKey[i % xorKey.length];
  }
  return buffer;
}

function parsePeHeaders(buf) {
  if (buf.length < 64) throw new Error('File too small');
  if (buf.readUInt16LE(0) !== 0x5A4D) throw new Error('Invalid DOS signature');

  const peOffset = buf.readUInt32LE(0x3C);
  if (peOffset + 24 > buf.length) throw new Error('Invalid PE offset');
  if (buf.readUInt32LE(peOffset) !== 0x00004550) throw new Error('Invalid PE signature');

  const numSections = buf.readUInt16LE(peOffset + 6);
  const optHeaderSize = buf.readUInt16LE(peOffset + 20);
  const optHeaderOff = peOffset + 24;

  if (optHeaderOff + optHeaderSize > buf.length) throw new Error('Optional header out of bounds');

  const magic = buf.readUInt16LE(optHeaderOff);
  const is64 = magic === 0x20B;

  const imageBase = is64
    ? Number(buf.readBigUInt64LE(optHeaderOff + 0x18))
    : buf.readUInt32LE(optHeaderOff + 0x1C);
  const sizeOfImage = buf.readUInt32LE(optHeaderOff + 0x38);
  const sizeOfHeaders = buf.readUInt32LE(optHeaderOff + 0x3C);
  const entryPoint = buf.readUInt32LE(optHeaderOff + 0x10);
  const ddOff = is64 ? optHeaderOff + 0x70 : optHeaderOff + 0x60;

  const dataDirectories = [];
  for (let i = 0; i < 16; i++) {
    dataDirectories.push({
      rva: buf.readUInt32LE(ddOff + i * 8),
      size: buf.readUInt32LE(ddOff + i * 8 + 4)
    });
  }

  const sections = [];
  const secOff = optHeaderOff + optHeaderSize;
  for (let i = 0; i < numSections; i++) {
    const off = secOff + i * 40;
    sections.push({
      name: buf.toString('ascii', off, off + 8).replace(/\0/g, ''),
      vSize: buf.readUInt32LE(off + 8),
      vAddr: buf.readUInt32LE(off + 12),
      rawSize: buf.readUInt32LE(off + 16),
      rawAddr: buf.readUInt32LE(off + 20),
      chars: buf.readUInt32LE(off + 36)
    });
  }

  return { is64, imageBase, sizeOfImage, sizeOfHeaders, entryPoint, dataDirectories, sections, optHeaderOff };
}

function readCString(buf, rva) {
  let str = '';
  let i = 0;
  while (rva + i < buf.length && buf[rva + i] !== 0) {
    str += String.fromCharCode(buf[rva + i]);
    i++;
  }
  return str;
}

function generateManifest(pe, mapped) {
  const lines = [];
  const ptrSize = pe.is64 ? 8 : 4;

  lines.push(`BASE ${pe.imageBase}`);
  lines.push(`IMAGE ${pe.sizeOfImage}`);
  lines.push(`EP ${pe.entryPoint}`);


  for (const sec of pe.sections) {
    lines.push(`SEC ${sec.name} ${sec.vAddr} ${sec.vSize} ${sec.chars}`);
  }


  const importDir = pe.dataDirectories[1];
  if (importDir.rva && importDir.size) {
    let off = importDir.rva;
    while (true) {
      const origFirstThunk = mapped.readUInt32LE(off);
      const nameRva = mapped.readUInt32LE(off + 12);
      const firstThunk = mapped.readUInt32LE(off + 16);
      if (!nameRva) break;

      const dllName = readCString(mapped, nameRva);
      lines.push(`DLL ${dllName}`);

      let thunkRva = origFirstThunk || firstThunk;
      let idx = 0;
      while (true) {
        const iatRva = firstThunk + idx * ptrSize;

        if (pe.is64) {
          const thunk = mapped.readBigUInt64LE(thunkRva);
          if (!thunk) break;

          if (thunk & 0x8000000000000000n) {
            lines.push(`FN ordinal ${Number(thunk & 0xFFFFn)} ${iatRva}`);
          } else {
            lines.push(`FN ${readCString(mapped, Number(thunk) + 2)} ${iatRva}`);
          }
        } else {
          const thunkAddr = mapped.readUInt32LE(thunkRva);
          if (!thunkAddr) break;

          if (thunkAddr & 0x80000000) {
            lines.push(`FN ordinal ${thunkAddr & 0xFFFF} ${iatRva}`);
          } else {
            lines.push(`FN ${readCString(mapped, thunkAddr + 2)} ${iatRva}`);
          }
        }

        thunkRva += ptrSize;
        idx++;
      }
      off += 20;
    }
  }


  const tlsDir = pe.dataDirectories[9];
  if (tlsDir.rva && tlsDir.size) {
    const tlsOff = tlsDir.rva;
    const callbacksVa = pe.is64
      ? Number(mapped.readBigUInt64LE(tlsOff + 0x18))
      : mapped.readUInt32LE(tlsOff + 0x0C);
    const callbacksRva = callbacksVa > pe.imageBase ? callbacksVa - pe.imageBase : 0;
    if (callbacksRva) {
      let cbOff = callbacksRva;
      while (true) {
        const cb = pe.is64
          ? Number(mapped.readBigUInt64LE(cbOff))
          : mapped.readUInt32LE(cbOff);
        if (!cb) break;
        lines.push(`TLS ${cb}`);
        cbOff += ptrSize;
      }
    }
  }


  const exportDir = pe.dataDirectories[0];
  if (exportDir.rva && exportDir.size) {
    const expOff = exportDir.rva;
    const numNames = mapped.readUInt32LE(expOff + 24);
    const namesRva = mapped.readUInt32LE(expOff + 32);
    const funcsRva = mapped.readUInt32LE(expOff + 28);
    const ordsRva = mapped.readUInt32LE(expOff + 36);
    const ordinalBase = mapped.readUInt32LE(expOff + 16);

    for (let i = 0; i < numNames; i++) {
      const nameRva = mapped.readUInt32LE(namesRva + i * 4);
      const name = readCString(mapped, nameRva);
      const ordinalIdx = mapped.readUInt16LE(ordsRva + i * 2);
      const funcRva = mapped.readUInt32LE(funcsRva + ordinalIdx * 4);
      lines.push(`EXP ${name} ${funcRva} ${ordinalBase + ordinalIdx}`);
    }
  }


  const relocDir = pe.dataDirectories[5];
  if (relocDir.rva && relocDir.size) {
    lines.push(`RELOC ${relocDir.rva} ${relocDir.size}`);
  }

  return lines.join('\n') + '\n';
}

function createMappedPe(rawPath, chosenBase) {
  const raw = fs.readFileSync(rawPath);
  const pe = parsePeHeaders(raw);


  const targetBase = chosenBase || pe.imageBase;
  const isReloc = targetBase !== pe.imageBase;

  console.log(`[MAPPER] PE type: ${pe.is64 ? 'PE32+' : 'PE32'}`);
  console.log(`[MAPPER] Original ImageBase: 0x${pe.imageBase.toString(16).toUpperCase()}, Target ImageBase: 0x${targetBase.toString(16).toUpperCase()}`);
  console.log(`[MAPPER] SizeOfImage: ${pe.sizeOfImage} bytes, SizeOfHeaders: ${pe.sizeOfHeaders}`);
  console.log(`[MAPPER] Sections: ${pe.sections.length}, EntryPoint: 0x${pe.entryPoint.toString(16).toUpperCase()}`);


  const mapped = Buffer.alloc(pe.sizeOfImage);


  raw.copy(mapped, 0, 0, pe.sizeOfHeaders);
  console.log(`[MAPPER] Copied headers: 0x${pe.sizeOfHeaders.toString(16)} bytes`);


  for (const sec of pe.sections) {
    if (sec.rawSize > 0) {
      const srcEnd = Math.min(sec.rawAddr + sec.rawSize, raw.length);
      const copyLen = srcEnd - sec.rawAddr;
      if (copyLen > 0) {
        raw.copy(mapped, sec.vAddr, sec.rawAddr, srcEnd);
        console.log(`[MAPPER] Section '${sec.name}': copied ${copyLen} bytes to VA 0x${sec.vAddr.toString(16).toUpperCase()}`);
      }
    }
  }


  const relocDir = pe.dataDirectories[5];
  if (relocDir.rva && relocDir.size && isReloc) {
    const delta = targetBase - pe.imageBase;
    let off = relocDir.rva;
    const end = relocDir.rva + relocDir.size;
    let fixCount = 0;

    console.log(`[MAPPER] Fixing relocations: delta = 0x${delta.toString(16).toUpperCase()}`);

    while (off < end) {
      const pageRva = mapped.readUInt32LE(off);
      const blockSize = mapped.readUInt32LE(off + 4);
      if (!blockSize) break;

      const count = (blockSize - 8) / 2;
      for (let i = 0; i < count; i++) {
        const entry = mapped.readUInt16LE(off + 8 + i * 2);
        const type = (entry >> 12) & 0xF;
        if (!type) continue;

        const addr = pageRva + (entry & 0xFFF);

        if (type === 3) {
          mapped.writeInt32LE(mapped.readInt32LE(addr) + delta, addr);
          fixCount++;
        } else if (type === 10) {
          const old = mapped.readBigInt64LE(addr);
          mapped.writeBigInt64LE(old + BigInt(delta), addr);
          fixCount++;
        } else if (type === 1) {
          mapped.writeUInt16LE(mapped.readUInt16LE(addr) + ((delta >> 16) & 0xFFFF), addr);
          fixCount++;
        } else if (type === 2) {
          mapped.writeUInt16LE(mapped.readUInt16LE(addr) + (delta & 0xFFFF), addr);
          fixCount++;
        }
      }
      off += blockSize;
    }
    console.log(`[MAPPER] Fixed ${fixCount} relocation entries`);
  } else if (relocDir.rva && relocDir.size) {
    console.log(`[MAPPER] Relocations present but targetBase === originalBase, no fix needed`);
  } else {
    console.log(`[MAPPER] No relocations directory`);
  }


  if (pe.is64) {
    mapped.writeBigUInt64LE(BigInt(targetBase), pe.optHeaderOff + 0x18);
  } else {
    mapped.writeUInt32LE(targetBase, pe.optHeaderOff + 0x1C);
  }
  console.log(`[MAPPER] Updated ImageBase in headers to 0x${targetBase.toString(16).toUpperCase()}`);

  return { mapped, targetBase, pe };
}

function nativeModule(body) {
  const name = typeof body?.name === 'string' ? body.name : 'module.dll';
  const access = authenticateAccessToken(body?.access_token, { op: 'module', name, timestamp_unix: body?.timestamp_unix, nonce: body?.nonce, signature: body?.signature });
  if (!access) {
    console.log('[MODULE] Rejected unauthenticated request');
    return nativeError(401, 'Unauthorized');
  }

  const resolvedModule = moduleFiles.resolve(name, path.join(PROJECT_ROOT, 'modules'));
  const modulePath = resolvedModule?.path;
  const requiredGame = requiredGameForModule(resolvedModule?.filename || name);
  if (requiredGame && !licenseAllowsGame(access.license, requiredGame)) {
    console.log(`[MODULE] Rejected ${requiredGame} module for key=${keyLabel(access.key)}`);
    return nativeError(403, 'License does not include this game');
  }

  if (!modulePath) return nativeError(404, 'Module not found');

  try {
    const { mapped, pe } = createMappedPe(modulePath, null);
    const manifest = generateManifest(pe, mapped);
    const manifestBuffer = Buffer.from(manifest, 'utf-8');
    const manifestLen = Buffer.alloc(4);
    manifestLen.writeUInt32LE(manifestBuffer.length, 0);

    const payload = Buffer.concat([manifestLen, manifestBuffer, mapped]);
    console.log(`[MODULE] Manifest: ${manifestBuffer.length} bytes, Mapped: ${mapped.length} bytes, Total: ${payload.length} bytes`);
    xorBuffer(payload, deriveXorKey(access.accessToken, access.hwid));
    console.log(`[MODULE] Streamed ${payload.length} bytes (session-masked, manifest+mapped) to key=${keyLabel(access.key)}`);
    return { status: 200, body: { success: true }, binary: payload };
  } catch (err) {
    console.error(`[MODULE] Error processing ${name}:`, err.message);
    return nativeError(500, 'Failed to process module');
  }
}

function nativeDriver(body) {
  const name = typeof body?.name === 'string' ? body.name : 'driver.sys';
  const access = authenticateAccessToken(body?.access_token, { op: 'driver', name, timestamp_unix: body?.timestamp_unix, nonce: body?.nonce, signature: body?.signature });
  if (!access) {
    console.log('[DRIVER] Rejected unauthenticated request');
    return nativeError(401, 'Unauthorized');
  }

  if (!/^[A-Za-z0-9][A-Za-z0-9._-]{0,122}\.sys$/i.test(name) || name.includes('..')) {
    return nativeError(400, 'Invalid driver name');
  }
  const replacementPath = name === 'driver.sys' ? path.join(path.resolve(process.env.CRISIS_UPLOAD_ROOT || path.join(PROJECT_ROOT, 'storage')), 'current', 'driver.sys') : null;
  const driverPath = replacementPath && fs.existsSync(replacementPath) ? replacementPath : path.join(PROJECT_ROOT, 'drivers', path.basename(name));

  if (!fs.existsSync(driverPath)) return nativeError(404, 'Driver not found');

  try {
    const raw = fs.readFileSync(driverPath);
    xorBuffer(raw, deriveXorKey(access.accessToken, access.hwid));
    console.log(`[DRIVER] Streamed ${raw.length} bytes (session-masked, raw driver) to key=${keyLabel(access.key)}`);
    return { status: 200, body: { success: true }, binary: raw };
  } catch (err) {
    console.error(`[DRIVER] Error processing ${name}:`, err.message);
    return nativeError(500, 'Failed to process driver');
  }
}

function handleNativeRequest(frame, ip) {
  const op = String(frame?.json?.op || '');
  if (op === 'authenticate') return authenticateNative(frame.json, ip);
  if (op === 'module') return nativeModule(frame.json);
  if (op === 'driver') return nativeDriver(frame.json);
  if (PLAYER_NATIVE_OPS[op]) return dispatchPlayerNative(frame.json);
  return nativeError(400, 'Unknown native operation');
}

function startNativeServer(port = NATIVE_PORT) {
  const server = createNativeServer(handleNativeRequest);
  server.listen(port, () => {
    const listening = server.address();
    const bound = typeof listening === 'object' && listening ? listening.port : port;
    console.log(`Native TCP auth listening on port ${bound}`);
  });
  return server;
}

function startServer(port = PORT, nativePort = NATIVE_PORT) {
  const nativeServer = startNativeServer(nativePort);
  const server = app.listen(port, () => {
  const startupDatabase = store.read();
  const licenseCount = Object.keys(startupDatabase.keys || {}).length;
  console.log(`Auth server running on ${process.env.NODE_ENV === 'production' ? 'HTTPS via trusted reverse proxy' : `http://localhost:${port}`}`);
  console.log(`Database: ${DB_PATH} (${licenseCount} license record${licenseCount === 1 ? '' : 's'} loaded)`);
  console.log(`Casino fairness: precommitted HMAC-SHA256 seeds backed by the operating-system CSPRNG; rate limit ${process.env.CRISIS_CASINO_RATE_MAX || 30}/${process.env.CRISIS_CASINO_RATE_WINDOW_MS || 10000}ms`);
  console.log(`Endpoints:`);
  console.log(`  TCP  authenticate/module/driver - Native client protocol on port ${nativePort}`);
  console.log(`  POST /admin/create-key          - Create a new key (body: {key, duration_seconds, games})`);
  console.log(`  POST /admin/upload-module       - Upload a module file (multipart)`);
  console.log(`  POST /admin/upload-driver       - Upload a driver file (multipart)`);
  console.log(`  POST /api/discord/interactions  - Signed Discord application commands`);
  console.log(`  TCP  player/casino              - Game client profile and casino ops`);
  console.log(`  GET  /v1/player                 - Authoritative profile and nearby leaderboard`);
  console.log(`  POST /v1/player/gameplay/*      - Idempotent match lifecycle events`);
  console.log(`  POST /v1/player/xp/earn         - Trusted-service XP award`);
  console.log(`  POST /v1/player/coins/purchase  - Exchange XP for Crisis Coins`);
  console.log(`  POST /v1/player/subscription/redeem-xp - Exchange XP for license time`);
  console.log(`  POST /v1/player/casino/*        - Provably-fair server casino`);
  });
  server.on('close', () => nativeServer.close());
  registerDiscordCommands().catch((error) => console.error('[DISCORD]', error.message));
  startDiscordPresence().catch((error) => console.error('[DISCORD]', error.message));
  return server;
}

mountAccountService(app, {
  store,
  secureCookie: process.env.CRISIS_COOKIE_SECURE !== 'false',
  verificationUrl: process.env.CRISIS_DEVICE_VERIFICATION_URL || '/device',
  allowedOrigins: String(process.env.CRISIS_CORS_ORIGINS || '').split(',').map((value) => value.trim()).filter(Boolean)
  ,storageRoot: process.env.CRISIS_UPLOAD_ROOT
  ,maxUploadBytes: process.env.CRISIS_UPLOAD_MAX_BYTES
});

if (require.main === module) startServer();

module.exports = { app, handleNativeRequest, loadDatabase, saveDatabase, startNativeServer, startServer, store };
