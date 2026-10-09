const crypto = require('crypto');
const { isLicensePaused } = require('../lib/license-state');

const REQUEST_FIELDS = [
  'license_hash',
  'hwid_hash',
  'discord_id',
  'discord_id_hash',
  'timestamp_unix',
  'nonce',
  'request_mac'
];

const ZERO = Buffer.from([0]);
const BASE64URL = /^[A-Za-z0-9_-]+$/;
const DISCORD_SNOWFLAKE = /^[0-9]{17,20}$/;

function sha256(parts) {
  const hash = crypto.createHash('sha256');
  for (const part of parts) hash.update(part);
  return hash.digest();
}

function hmacSha256(key, value) {
  return crypto.createHmac('sha256', key).update(value).digest();
}

function delimitedUtf8(...values) {
  const parts = [];
  values.forEach((value, index) => {
    parts.push(Buffer.from(value, 'utf8'));
    if (index !== values.length - 1) parts.push(ZERO);
  });
  return parts;
}

function base64url(value) {
  return Buffer.from(value).toString('base64url');
}

function decodeBase64url32(value) {
  if (typeof value !== 'string' || value.length !== 43 || !BASE64URL.test(value)) return null;

  try {
    const decoded = Buffer.from(value, 'base64url');
    if (decoded.length !== 32 || decoded.toString('base64url') !== value) return null;
    return decoded;
  } catch {
    return null;
  }
}

function timingSafeEqual(left, right) {
  return Buffer.isBuffer(left) && Buffer.isBuffer(right) && left.length === right.length && crypto.timingSafeEqual(left, right);
}

function licenseHash(licenseKey) {
  return sha256(delimitedUtf8('discord-license:v2', licenseKey));
}

function hwidHash(hwid) {
  return sha256(delimitedUtf8('discord-hwid:v2', hwid));
}

function discordIdHash(discordId, licenseHashValue, hwidHashValue, nonce, timestampUnix) {
  return sha256(delimitedUtf8(
    'discord-user:v2',
    discordId,
    licenseHashValue,
    hwidHashValue,
    nonce,
    String(timestampUnix)
  ));
}

function requestAuthKey(licenseKey, hwid) {
  return sha256(delimitedUtf8('discord-request-auth:v2', licenseKey, hwid));
}

function canonicalRequest(body) {
  return [
    'POST',
    '/v2/discord/member-info',
    body.license_hash,
    body.hwid_hash,
    body.discord_id,
    body.discord_id_hash,
    String(body.timestamp_unix),
    body.nonce
  ].join('\n');
}

function validateRequestShape(body) {
  if (!body || typeof body !== 'object' || Array.isArray(body)) return null;

  const keys = Object.keys(body);
  if (keys.length !== REQUEST_FIELDS.length || keys.some((key) => !REQUEST_FIELDS.includes(key))) return null;
  if (REQUEST_FIELDS.some((key) => !Object.prototype.hasOwnProperty.call(body, key))) return null;
  if (typeof body.discord_id !== 'string' || !DISCORD_SNOWFLAKE.test(body.discord_id)) return null;
  if (typeof body.timestamp_unix !== 'number' || !Number.isSafeInteger(body.timestamp_unix)) return null;

  const licenseHashBytes = decodeBase64url32(body.license_hash);
  const hwidHashBytes = decodeBase64url32(body.hwid_hash);
  const discordIdHashBytes = decodeBase64url32(body.discord_id_hash);
  const nonceBytes = decodeBase64url32(body.nonce);
  const requestMacBytes = decodeBase64url32(body.request_mac);
  if (!licenseHashBytes || !hwidHashBytes || !discordIdHashBytes || !nonceBytes || !requestMacBytes) return null;

  return {
    ...body,
    licenseHashBytes,
    hwidHashBytes,
    discordIdHashBytes,
    nonceBytes,
    requestMacBytes
  };
}

function authenticateDiscordRequest(request, database, nowUnix) {
  if (Math.abs(nowUnix - request.timestamp_unix) > 60) return null;
  if (!database || !database.keys || typeof database.keys !== 'object') return null;

  let matchedKey = null;
  let matchedRecord = null;
  for (const [licenseKey, record] of Object.entries(database.keys)) {
    const candidate = licenseHash(licenseKey);
    if (timingSafeEqual(candidate, request.licenseHashBytes) && matchedKey === null) {
      matchedKey = licenseKey;
      matchedRecord = record;
    }
  }

  if (!matchedRecord || matchedRecord.start_unix === null || matchedRecord.start_unix === undefined) return null;
  if (isLicensePaused(database, matchedRecord)) return null;
  if (typeof matchedRecord.expiry_unix !== 'number' || nowUnix >= matchedRecord.expiry_unix) return null;
  if (typeof matchedRecord.hwid !== 'string' || !matchedRecord.hwid) return null;
  if (matchedRecord.discord_id !== request.discord_id || !DISCORD_SNOWFLAKE.test(matchedRecord.discord_id)) return null;

  const expectedHwidHash = hwidHash(matchedRecord.hwid);
  if (!timingSafeEqual(expectedHwidHash, request.hwidHashBytes)) return null;

  const canonical = canonicalRequest(request);
  const expectedMac = hmacSha256(requestAuthKey(matchedKey, matchedRecord.hwid), Buffer.from(canonical, 'utf8'));
  if (!timingSafeEqual(expectedMac, request.requestMacBytes)) return null;

  const expectedDiscordIdHash = discordIdHash(
    request.discord_id,
    request.license_hash,
    request.hwid_hash,
    request.nonce,
    request.timestamp_unix
  );
  if (!timingSafeEqual(expectedDiscordIdHash, request.discordIdHashBytes)) return null;

  return {
    canonical,
    requestHash: base64url(sha256([Buffer.from(canonical, 'utf8')])),
    licenseKey: matchedKey,
    record: matchedRecord
  };
}

module.exports = {
  authenticateDiscordRequest,
  base64url,
  canonicalRequest,
  decodeBase64url32,
  discordIdHash,
  hmacSha256,
  hwidHash,
  licenseHash,
  requestAuthKey,
  sha256,
  timingSafeEqual,
  validateRequestShape
};
