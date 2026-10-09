const crypto = require('crypto');

const EMAIL = /^[^\s@]{1,64}@[^\s@]{1,190}$/;

function b64(value) { return Buffer.from(value).toString('base64url'); }
function digest(value, context = '') { return b64(crypto.createHash('sha256').update(context).update('\0').update(value).digest()); }
function randomToken(bytes = 32) { return crypto.randomBytes(bytes).toString('base64url'); }
function safeEqual(left, right) {
  if (typeof left !== 'string' || typeof right !== 'string') return false;
  const a = Buffer.from(left); const b = Buffer.from(right);
  return a.length === b.length && crypto.timingSafeEqual(a, b);
}
function normalizeEmail(value) {
  const email = typeof value === 'string' ? value.trim().toLowerCase() : '';
  return EMAIL.test(email) ? email : null;
}
function validatePassword(value) {
  return typeof value === 'string' && value.length >= 12 && value.length <= 128 && /[a-z]/i.test(value) && /[^a-z]/i.test(value);
}
function scrypt(password, salt, options) {
  return new Promise((resolve, reject) => crypto.scrypt(password, salt, 32, options, (error, key) => error ? reject(error) : resolve(key)));
}
async function hashPassword(password) {
  const salt = crypto.randomBytes(16);
  const options = { N: 32768, r: 8, p: 1, maxmem: 64 * 1024 * 1024 };
  const key = await scrypt(password, salt, options);
  return `scrypt$32768$8$1$${b64(salt)}$${b64(key)}`;
}
async function verifyPassword(password, encoded) {
  try {
    const [name, n, r, p, salt, expected] = String(encoded).split('$');
    if (name !== 'scrypt' || n !== '32768' || r !== '8' || p !== '1') return false;
    const key = await scrypt(password, Buffer.from(salt, 'base64url'), { N: Number(n), r: Number(r), p: Number(p), maxmem: 64 * 1024 * 1024 });
    return safeEqual(b64(key), expected);
  } catch { return false; }
}

function clientIp(req) { return String(req.ip || req.socket?.remoteAddress || 'unknown').slice(0, 64); }
function requestId(req) { return String(req.id || req.get('x-request-id') || crypto.randomUUID()).slice(0, 128); }
function metadata(req) { return { ip: clientIp(req), user_agent: String(req.get('user-agent') || '').slice(0, 256) }; }

function createRateLimiter() {
  const buckets = new Map();
  return function limit(key, maximum, windowMs) {
    const now = Date.now();
    const cleanKey = String(key).slice(0, 256);
    const entry = buckets.get(cleanKey);
    if (!entry || entry.resetAt <= now) {
      buckets.set(cleanKey, { count: 1, resetAt: now + windowMs });
      return { allowed: true };
    }
    if (entry.count >= maximum) return { allowed: false, retryAfter: Math.max(1, Math.ceil((entry.resetAt - now) / 1000)) };
    entry.count += 1;
    return { allowed: true };
  };
}

module.exports = { clientIp, createRateLimiter, digest, hashPassword, metadata, normalizeEmail, randomToken, requestId, safeEqual, validatePassword, verifyPassword };
