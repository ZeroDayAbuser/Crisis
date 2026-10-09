const crypto = require('crypto');

const AUTH_DOMAIN = 'crisis-client-auth:v1';
const ARTIFACT_DOMAIN = 'crisis-artifact-proof:v1';
const PUBLIC_BLOB_BYTES = 72;
const PUBLIC_BLOB_MAGIC = 0x31534345;
const NONCE = /^[A-Za-z0-9_-]{43}$/;
const SIGNATURE = /^[A-Za-z0-9_-]{86}$/;
const PUBLIC_KEY = /^[A-Za-z0-9_-]{96}$/;
const MAX_CLOCK_SKEW_SECONDS = 60;
const MAX_REPLAYS = 20_000;

function decodeExact(value, pattern, bytes) {
  if (typeof value !== 'string' || !pattern.test(value)) return null;
  try {
    const decoded = Buffer.from(value, 'base64url');
    return decoded.length === bytes && decoded.toString('base64url') === value ? decoded : null;
  } catch {
    return null;
  }
}

function importPublicKey(encoded) {
  const blob = decodeExact(encoded, PUBLIC_KEY, PUBLIC_BLOB_BYTES);
  if (!blob || blob.readUInt32LE(0) !== PUBLIC_BLOB_MAGIC || blob.readUInt32LE(4) !== 32) return null;
  try {
    return crypto.createPublicKey({
      key: {
        kty: 'EC', crv: 'P-256',
        x: blob.subarray(8, 40).toString('base64url'),
        y: blob.subarray(40, 72).toString('base64url')
      },
      format: 'jwk'
    });
  } catch {
    return null;
  }
}

function authCanonical(key, hwid, timestamp, nonce) {
  return [AUTH_DOMAIN, String(timestamp), nonce, hwid, key].join('\n');
}

function artifactCanonical(operation, name, accessToken, timestamp, nonce) {
  return [ARTIFACT_DOMAIN, String(operation || '').toLowerCase(), String(name || ''), accessToken, String(timestamp), nonce].join('\n');
}

function verifySignature(publicKey, canonical, encodedSignature) {
  const signature = decodeExact(encodedSignature, SIGNATURE, 64);
  if (!publicKey || !signature) return false;
  try {
    return crypto.verify('sha256', Buffer.from(canonical, 'utf8'), { key: publicKey, dsaEncoding: 'ieee-p1363' }, signature);
  } catch {
    return false;
  }
}

function createReplayCache(now = () => Math.floor(Date.now() / 1000)) {
  const entries = new Map();
  return function consume(scope, nonce, timestamp) {
    const current = now();
    if (!Number.isSafeInteger(timestamp) || Math.abs(current - timestamp) > MAX_CLOCK_SKEW_SECONDS || !NONCE.test(nonce || '')) return false;
    for (const [key, expiresAt] of entries) if (expiresAt <= current) entries.delete(key);
    if (entries.size >= MAX_REPLAYS) return false;
    const replayKey = `${scope}\0${nonce}`;
    if (entries.has(replayKey)) return false;
    entries.set(replayKey, current + MAX_CLOCK_SKEW_SECONDS + 1);
    return true;
  };
}

function createClientProofVerifier(options = {}) {
  const consume = createReplayCache(options.now);

  function verifyAuthentication(body) {
    if (!body || typeof body !== 'object') return null;
    const publicKey = importPublicKey(body.client_public_key);
    if (!publicKey || !verifySignature(publicKey, authCanonical(body.key, body.hwid, body.timestamp_unix, body.nonce), body.signature)) return null;
    const fingerprint = crypto.createHash('sha256').update(Buffer.from(body.client_public_key, 'base64url')).digest('base64url');
    if (!consume(`auth:${fingerprint}`, body.nonce, body.timestamp_unix)) return null;
    return { encodedPublicKey: body.client_public_key, publicKey, fingerprint };
  }

  function verifyArtifact(proof, accessToken, encodedPublicKey) {
    if (!proof || typeof proof !== 'object') return false;
    const operation = String(proof.op || '');
    const name = String(proof.name || '');
    const timestamp = Number(proof.timestamp_unix);
    const nonce = String(proof.nonce || '');
    const signature = String(proof.signature || '');
    if (operation !== 'module' && operation !== 'driver') return false;
    const publicKey = importPublicKey(encodedPublicKey);
    if (!publicKey || !verifySignature(publicKey, artifactCanonical(operation, name, accessToken, timestamp, nonce), signature)) return false;
    return consume(`artifact:${accessToken}`, nonce, timestamp);
  }

  return { verifyArtifact, verifyAuthentication };
}

module.exports = { artifactCanonical, authCanonical, createClientProofVerifier, importPublicKey };
