const crypto = require('crypto');
const fs = require('fs');

const CNG_ECDH_PUBLIC_P256_MAGIC = 0x314B4345;
const ECDH_PUBLIC_BLOB_BYTES = 72;
const AES_GCM_IV_BYTES = 12;
const AES_GCM_TAG_BYTES = 16;
const ENVELOPE_VERSION = 1;
const MAX_REQUEST_CIPHERTEXT_BYTES = 5800;
const MAX_RESPONSE_CIPHERTEXT_BYTES = 48 * 1024;
const BASE64URL = /^[A-Za-z0-9_-]+$/;
const REQUEST_AAD = Buffer.from('discord-member-info/v2/request', 'utf8');
const RESPONSE_AAD = Buffer.from('discord-member-info/v2/response', 'utf8');
const ZERO = Buffer.from([0]);

function decodeBase64url(value, minimum, maximum) {
  if (typeof value !== 'string' || !value || !BASE64URL.test(value)) return null;
  try {
    const bytes = Buffer.from(value, 'base64url');
    if (bytes.length < minimum || bytes.length > maximum || bytes.toString('base64url') !== value) return null;
    return bytes;
  } catch {
    return null;
  }
}

function parseRequestEnvelope(body) {
  if (!body || typeof body !== 'object' || Array.isArray(body)) return null;
  const keys = Object.keys(body);
  if (keys.length !== 2 || !Object.prototype.hasOwnProperty.call(body, 'e') || !Object.prototype.hasOwnProperty.call(body, 'd')) return null;

  const publicBlob = decodeBase64url(body.e, ECDH_PUBLIC_BLOB_BYTES, ECDH_PUBLIC_BLOB_BYTES);
  const encrypted = decodeBase64url(body.d, 1 + AES_GCM_IV_BYTES + AES_GCM_TAG_BYTES, MAX_REQUEST_CIPHERTEXT_BYTES + 1 + AES_GCM_IV_BYTES + AES_GCM_TAG_BYTES);
  if (!publicBlob || !encrypted || publicBlob.readUInt32LE(0) !== CNG_ECDH_PUBLIC_P256_MAGIC || publicBlob.readUInt32LE(4) !== 32) return null;
  return { publicBlob, encrypted };
}

function loadTransportPrivateKey(file) {
  const key = crypto.createPrivateKey(fs.readFileSync(file, 'utf8'));
  const jwk = key.export({ format: 'jwk' });
  if (key.asymmetricKeyType !== 'ec' || jwk.kty !== 'EC' || jwk.crv !== 'P-256') throw new Error('Invalid Discord transport key');
  return key;
}

function importClientPublicKey(publicBlob) {
  return crypto.createPublicKey({
    key: {
      kty: 'EC',
      crv: 'P-256',
      x: publicBlob.subarray(8, 40).toString('base64url'),
      y: publicBlob.subarray(40, 72).toString('base64url')
    },
    format: 'jwk'
  });
}

function deriveKey(label, sharedSecret, nonce) {
  const hash = crypto.createHash('sha256');
  hash.update(label, 'utf8');
  hash.update(ZERO);
  hash.update(sharedSecret);
  if (nonce !== undefined) {
    hash.update(ZERO);
    hash.update(nonce, 'utf8');
  }
  return hash.digest();
}

function encrypt(key, plaintext, aad) {
  const iv = crypto.randomBytes(AES_GCM_IV_BYTES);
  const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
  cipher.setAAD(aad);
  const ciphertext = Buffer.concat([cipher.update(plaintext), cipher.final()]);
  const tag = cipher.getAuthTag();
  return Buffer.concat([Buffer.from([ENVELOPE_VERSION]), iv, ciphertext, tag]).toString('base64url');
}

function decrypt(key, encoded, aad, maximumCiphertextBytes = MAX_RESPONSE_CIPHERTEXT_BYTES) {
  const payload = typeof encoded === 'string' ? decodeBase64url(encoded, 1 + AES_GCM_IV_BYTES + AES_GCM_TAG_BYTES, maximumCiphertextBytes + 1 + AES_GCM_IV_BYTES + AES_GCM_TAG_BYTES) : encoded;
  if (!payload || payload[0] !== ENVELOPE_VERSION) return null;

  const iv = payload.subarray(1, 1 + AES_GCM_IV_BYTES);
  const tag = payload.subarray(payload.length - AES_GCM_TAG_BYTES);
  const ciphertext = payload.subarray(1 + AES_GCM_IV_BYTES, payload.length - AES_GCM_TAG_BYTES);
  try {
    const decipher = crypto.createDecipheriv('aes-256-gcm', key, iv);
    decipher.setAAD(aad);
    decipher.setAuthTag(tag);
    return Buffer.concat([decipher.update(ciphertext), decipher.final()]);
  } catch {
    return null;
  }
}

function decryptRequestEnvelope(envelope, transportPrivateKey) {
  try {
    const clientKey = importClientPublicKey(envelope.publicBlob);
    const sharedSecret = crypto.diffieHellman({ privateKey: transportPrivateKey, publicKey: clientKey });
    const plaintext = decrypt(deriveKey('discord-envelope-request:v1', sharedSecret), envelope.encrypted, REQUEST_AAD, MAX_REQUEST_CIPHERTEXT_BYTES);
    if (!plaintext) return null;
    return { body: JSON.parse(plaintext.toString('utf8')), sharedSecret };
  } catch {
    return null;
  }
}

function encryptResponseEnvelope(attestation, sharedSecret, nonce) {
  return {
    d: encrypt(
      deriveKey('discord-envelope-response:v1', sharedSecret, nonce),
      Buffer.from(attestation, 'utf8'),
      RESPONSE_AAD
    )
  };
}

module.exports = {
  CNG_ECDH_PUBLIC_P256_MAGIC,
  ECDH_PUBLIC_BLOB_BYTES,
  decrypt,
  decryptRequestEnvelope,
  deriveKey,
  encrypt,
  encryptResponseEnvelope,
  loadTransportPrivateKey,
  parseRequestEnvelope
};
