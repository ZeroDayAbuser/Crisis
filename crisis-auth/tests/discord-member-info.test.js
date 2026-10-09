const assert = require('assert/strict');
const crypto = require('crypto');
const express = require('express');
const fs = require('fs');
const http = require('http');
const os = require('os');
const path = require('path');

const { createDiscordMemberRouter, createRateLimiter, createReplayCache } = require('../src/discord/member-route');
const { DiscordApiError, DiscordMemberService, defaultAvatarUrl } = require('../src/discord/member-service');
const { CNG_ECDH_PUBLIC_P256_MAGIC, decrypt, deriveKey, encrypt } = require('../src/discord/envelope');
const {
  base64url,
  canonicalRequest,
  discordIdHash,
  hmacSha256,
  hwidHash,
  licenseHash,
  requestAuthKey
} = require('../src/discord/request-auth');
const { signDiscordAttestation, verifyDiscordAttestation } = require('../src/discord/response-signing');

const NOW_UNIX = 1783800000;
const NOW_MS = NOW_UNIX * 1000;
const GUILD_ID = '123456789012345678';
const DISCORD_ID = '987654321098765432';
const LICENSE_KEY = 'VALID-LICENSE';
const HWID = 'VALID-HWID';
const ISSUER = 'crisis-auth-discord-member-info';
const AUDIENCE = 'windows-client';
const KID = 'test-es256-key';

function fakeMember(overrides = {}) {
  return {
    user: {
      id: DISCORD_ID,
      username: 'discord-user',
      global_name: 'Global name',
      avatar: 'user-avatar'
    },
    nick: 'Guild nickname',
    avatar: 'guild-avatar',
    roles: ['222222222222222222', '333333333333333333'],
    ...overrides
  };
}

function fakeRoles() {
  return [
    { id: GUILD_ID, name: '@everyone', position: 0, color: 0, hoist: false, managed: false, mentionable: false },
    { id: '222222222222222222', name: 'Premium', position: 12, color: 16711680, hoist: true, managed: false, mentionable: false },
    { id: '333333333333333333', name: 'Supporter', position: 12, color: 255, hoist: false, managed: false, mentionable: true },
    { id: '444444444444444444', name: 'Unused', position: 99, color: 0, hoist: false, managed: false, mentionable: false }
  ];
}

function response(status, body, headers = {}) {
  return { status, body, headers };
}

function createFakeDiscord(mode = {}) {
  const calls = { members: 0, roles: 0 };
  const fetchDiscord = async (requestPath) => {
    if (requestPath.endsWith('/roles')) {
      calls.roles += 1;
      if (mode.rolesError) return mode.rolesError;
      return response(200, mode.roles || fakeRoles());
    }
    calls.members += 1;
    if (mode.memberError) return mode.memberError;
    return response(200, mode.member || fakeMember());
  };
  return { fetchDiscord, calls };
}

function testDatabase() {
  return {
    keys: {
      [LICENSE_KEY]: { start_unix: NOW_UNIX - 300, expiry_unix: NOW_UNIX + 3600, hwid: HWID, discord_id: DISCORD_ID },
      'UNACTIVATED-LICENSE': { start_unix: null, expiry_unix: null, hwid: null },
      'EXPIRED-LICENSE': { start_unix: NOW_UNIX - 7200, expiry_unix: NOW_UNIX - 1, hwid: HWID, discord_id: DISCORD_ID },
      'PAUSED-LICENSE': { start_unix: NOW_UNIX - 300, expiry_unix: NOW_UNIX + 3600, hwid: HWID, discord_id: DISCORD_ID, pause_reasons: ['manual'] }
    }
  };
}

let nonceCounter = 0;
let transportPublicKey;
function makeNonce() {
  const nonce = Buffer.alloc(32, nonceCounter++ % 256);
  return nonce.toString('base64url');
}

function makeInnerRequest(options = {}) {
  const licenseKey = options.licenseKey || LICENSE_KEY;
  const hwid = options.hwid || HWID;
  const discordId = options.discordId || DISCORD_ID;
  const timestamp = options.timestamp === undefined ? NOW_UNIX : options.timestamp;
  const nonce = options.nonce || makeNonce();
  const licenseHashValue = base64url(licenseHash(licenseKey));
  const hwidHashValue = base64url(hwidHash(hwid));
  const discordIdHashValue = base64url(discordIdHash(discordId, licenseHashValue, hwidHashValue, nonce, timestamp));
  const body = {
    license_hash: licenseHashValue,
    hwid_hash: hwidHashValue,
    discord_id: discordId,
    discord_id_hash: discordIdHashValue,
    timestamp_unix: timestamp,
    nonce,
    request_mac: ''
  };
  body.request_mac = base64url(hmacSha256(requestAuthKey(licenseKey, hwid), Buffer.from(canonicalRequest(body), 'utf8')));
  return { ...body, ...options.overrides };
}

function encryptRequest(inner) {
  const pair = crypto.generateKeyPairSync('ec', { namedCurve: 'prime256v1' });
  const publicJwk = pair.publicKey.export({ format: 'jwk' });
  const publicBlob = Buffer.alloc(72);
  publicBlob.writeUInt32LE(CNG_ECDH_PUBLIC_P256_MAGIC, 0);
  publicBlob.writeUInt32LE(32, 4);
  Buffer.from(publicJwk.x, 'base64url').copy(publicBlob, 8);
  Buffer.from(publicJwk.y, 'base64url').copy(publicBlob, 40);
  const sharedSecret = crypto.diffieHellman({ privateKey: pair.privateKey, publicKey: transportPublicKey });
  return {
    outer: {
      e: publicBlob.toString('base64url'),
      d: encrypt(deriveKey('discord-envelope-request:v1', sharedSecret), Buffer.from(JSON.stringify(inner), 'utf8'), Buffer.from('discord-member-info/v2/request', 'utf8'))
    },
    inner,
    sharedSecret
  };
}

function makeRequest(options = {}) {
  return encryptRequest(makeInnerRequest(options));
}

function decryptAttestation(response, request) {
  assert.equal(typeof response.body.d, 'string');
  const plaintext = decrypt(
    deriveKey('discord-envelope-response:v1', request.sharedSecret, request.inner.nonce),
    response.body.d,
    Buffer.from('discord-member-info/v2/response', 'utf8')
  );
  assert.ok(plaintext);
  return plaintext.toString('utf8');
}

async function startRoute(options) {
  const app = express();
  app.use('/v2/discord/member-info', createDiscordMemberRouter(options));
  const server = await new Promise((resolve) => {
    const instance = app.listen(0, '127.0.0.1', () => resolve(instance));
  });
  return {
    port: server.address().port,
    close: () => new Promise((resolve, reject) => server.close((error) => error ? reject(error) : resolve()))
  };
}

function postJson(port, body, contentType = 'application/json') {
  const payload = typeof body === 'string' ? body : JSON.stringify(body);
  return new Promise((resolve, reject) => {
    const request = http.request({
      hostname: '127.0.0.1',
      port,
      path: '/v2/discord/member-info',
      method: 'POST',
      headers: {
        'Content-Type': contentType,
        'Content-Length': Buffer.byteLength(payload)
      }
    }, (responseValue) => {
      const chunks = [];
      responseValue.on('data', (chunk) => chunks.push(chunk));
      responseValue.on('end', () => {
        const text = Buffer.concat(chunks).toString('utf8');
        resolve({ status: responseValue.statusCode, headers: responseValue.headers, body: text ? JSON.parse(text) : null });
      });
    });
    request.on('error', reject);
    request.end(payload);
  });
}

async function expectStatus(port, body, status) {
  const result = await postJson(port, body && body.outer ? body.outer : body);
  assert.equal(result.status, status, JSON.stringify(result.body));
  return result;
}

async function expectReject(action) {
  await assert.rejects(action);
}

async function main() {
  const { privateKey, publicKey } = crypto.generateKeyPairSync('ec', { namedCurve: 'prime256v1' });
  const transportPair = crypto.generateKeyPairSync('ec', { namedCurve: 'prime256v1' });
  transportPublicKey = transportPair.publicKey;
  const keyDirectory = fs.mkdtempSync(path.join(os.tmpdir(), 'discord-member-info-'));
  const privateKeyFile = path.join(keyDirectory, 'signing-private-key.pem');
  const transportKeyFile = path.join(keyDirectory, 'transport-private-key.pem');
  fs.writeFileSync(privateKeyFile, privateKey.export({ type: 'pkcs8', format: 'pem' }));
  fs.writeFileSync(transportKeyFile, transportPair.privateKey.export({ type: 'pkcs8', format: 'pem' }));
  const publicKeyPem = publicKey.export({ type: 'spki', format: 'pem' });
  const config = {
    botToken: 'test-bot-token-not-a-secret',
    guildId: GUILD_ID,
    apiTimeoutMs: 5000,
    roleCacheMs: 30000,
    signingPrivateKeyFile: privateKeyFile,
    transportPrivateKeyFile: transportKeyFile,
    signingKeyId: KID,
    signingIssuer: ISSUER,
    signingAudience: AUDIENCE
  };
  const fake = createFakeDiscord();
  const service = new DiscordMemberService({ fetchDiscord: fake.fetchDiscord, now: () => NOW_MS });
  const route = await startRoute({
    loadDatabase: testDatabase,
    getConfig: () => config,
    discordService: service,
    now: () => NOW_MS,
    rateLimiter: createRateLimiter({ now: () => NOW_MS, ipLimit: 1000, licenseLimit: 1000 }),
    replayCache: createReplayCache({ now: () => NOW_MS })
  });

  try {
    const unavailableRoute = await startRoute({
      loadDatabase: testDatabase,
      getConfig: () => ({ guildId: GUILD_ID }),
      now: () => NOW_MS
    });
    try {
      const unavailable = await expectStatus(unavailableRoute.port, makeRequest(), 503);
      assert.deepEqual(unavailable.body, { success: false, error: 'Discord service unavailable' });
    } finally {
      await unavailableRoute.close();
    }

    let valid = makeRequest();
    let result = await expectStatus(route.port, valid, 200);
    assert.deepEqual(Object.keys(result.body), ['d']);
    assert.equal(JSON.stringify(valid.outer).includes('license_hash'), false);
    assert.equal(JSON.stringify(valid.outer).includes(DISCORD_ID), false);
    assert.equal(JSON.stringify(result.body).includes('discord-user'), false);
    const verified = await verifyDiscordAttestation(decryptAttestation(result, valid), publicKeyPem, { issuer: ISSUER, audience: AUDIENCE, kid: KID }, NOW_UNIX);
    assert.equal(verified.payload.member, true);
    assert.equal(verified.payload.discord_id, DISCORD_ID);
    assert.equal(verified.payload.username, 'discord-user');
    assert.equal(verified.payload.display_name, 'Guild nickname');
    assert.equal(verified.payload.pfp_url, `https://cdn.discordapp.com/guilds/${GUILD_ID}/users/${DISCORD_ID}/avatars/guild-avatar.webp?size=256`);
    assert.equal(verified.payload.roles.length, 2);
    assert.equal(verified.payload.roles[0].id, '333333333333333333');
    assert.equal(verified.payload.highest_role.id, '333333333333333333');

    await expectStatus(route.port, { ...makeRequest().outer, unexpected: true }, 400);
    await expectStatus(route.port, makeRequest({ overrides: { discord_id: 123456789012345678 } }), 400);
    await expectStatus(route.port, makeRequest({ overrides: { discord_id: '1234abc' } }), 400);
    await expectStatus(route.port, makeRequest({ overrides: { license_hash: 'not-base64url' } }), 400);
    await expectStatus(route.port, makeRequest({ overrides: { hwid_hash: Buffer.alloc(31).toString('base64url') } }), 400);
    await expectStatus(route.port, makeRequest({ overrides: { request_mac: Buffer.alloc(33).toString('base64url') } }), 400);
    const tamperedEnvelope = makeRequest();
    tamperedEnvelope.outer.d = `${tamperedEnvelope.outer.d.startsWith('A') ? 'B' : 'A'}${tamperedEnvelope.outer.d.slice(1)}`;
    await expectStatus(route.port, tamperedEnvelope, 401);
    await expectStatus(route.port, makeRequest({ timestamp: NOW_UNIX - 61 }), 401);
    await expectStatus(route.port, makeRequest({ timestamp: NOW_UNIX + 61 }), 401);
    const missingNonce = makeInnerRequest();
    delete missingNonce.nonce;
    await expectStatus(route.port, encryptRequest(missingNonce), 400);
    await expectStatus(route.port, makeRequest({ overrides: { request_mac: Buffer.alloc(32).toString('base64url') } }), 401);
    await expectStatus(route.port, '{', 400);

    valid = makeRequest();
    await expectStatus(route.port, valid, 200);
    await expectStatus(route.port, valid, 401);
    await expectStatus(route.port, makeRequest({ licenseKey: 'UNKNOWN-LICENSE' }), 401);
    await expectStatus(route.port, makeRequest({ hwid: 'WRONG-HWID' }), 401);
    await expectStatus(route.port, makeRequest({ licenseKey: 'UNACTIVATED-LICENSE', hwid: 'UNACTIVATED-HWID' }), 401);
    await expectStatus(route.port, makeRequest({ licenseKey: 'EXPIRED-LICENSE' }), 401);
    await expectStatus(route.port, makeRequest({ licenseKey: 'PAUSED-LICENSE' }), 401);
    await expectStatus(route.port, makeRequest({ discordId: '111111111111111111' }), 401);
    const originalInner = makeInnerRequest();
    await expectStatus(route.port, encryptRequest({ ...originalInner, discord_id: '111111111111111111' }), 401);
    await expectStatus(route.port, encryptRequest({ ...originalInner, nonce: makeNonce() }), 401);
    await expectStatus(route.port, encryptRequest({ ...originalInner, timestamp_unix: NOW_UNIX + 1 }), 401);
    await expectStatus(route.port, encryptRequest({ ...originalInner, discord_id_hash: makeNonce() }), 401);

    const originalTimingSafeEqual = crypto.timingSafeEqual;
    let timingSafeCalls = 0;
    crypto.timingSafeEqual = (...args) => { timingSafeCalls += 1; return originalTimingSafeEqual(...args); };
    try {
      await expectStatus(route.port, makeRequest(), 200);
    } finally {
      crypto.timingSafeEqual = originalTimingSafeEqual;
    }
    assert.ok(timingSafeCalls >= 4);

    const absentService = new DiscordMemberService({
      now: () => NOW_MS,
      fetchDiscord: async () => response(404, { code: 10007 })
    });
    const absentRoute = await startRoute({
      loadDatabase: testDatabase,
      getConfig: () => config,
      discordService: absentService,
      now: () => NOW_MS,
      rateLimiter: createRateLimiter({ now: () => NOW_MS, ipLimit: 1000, licenseLimit: 1000 }),
      replayCache: createReplayCache({ now: () => NOW_MS })
    });
    try {
      const absentRequest = makeRequest();
      result = await expectStatus(absentRoute.port, absentRequest, 200);
      const absent = await verifyDiscordAttestation(decryptAttestation(result, absentRequest), publicKeyPem, { issuer: ISSUER, audience: AUDIENCE, kid: KID }, NOW_UNIX);
      assert.equal(absent.payload.member, false);
      assert.deepEqual(absent.payload.roles, []);
      assert.equal(absent.payload.highest_role, null);
      assert.equal(absent.payload.pfp_url, null);
    } finally {
      await absentRoute.close();
    }

    const mappingFake = createFakeDiscord();
    const mappingService = new DiscordMemberService({ fetchDiscord: mappingFake.fetchDiscord, now: () => NOW_MS });
    let profile = await mappingService.getMemberProfile(config, DISCORD_ID);
    assert.equal(profile.display_name, 'Guild nickname');
    assert.equal(profile.global_display_name, 'Global name');
    assert.equal(profile.roles[0].id, '333333333333333333');
    assert.equal(profile.highest_role.id, '333333333333333333');
    await mappingService.getMemberProfile(config, DISCORD_ID);
    assert.equal(mappingFake.calls.roles, 1);
    const globalFallback = new DiscordMemberService({ fetchDiscord: createFakeDiscord({ member: fakeMember({ nick: null, avatar: null }) }).fetchDiscord, now: () => NOW_MS });
    profile = await globalFallback.getMemberProfile(config, DISCORD_ID);
    assert.equal(profile.display_name, 'Global name');
    assert.equal(profile.pfp_url, `https://cdn.discordapp.com/avatars/${DISCORD_ID}/user-avatar.webp?size=256`);
    const usernameFallback = new DiscordMemberService({ fetchDiscord: createFakeDiscord({ member: fakeMember({ nick: null, avatar: null, user: { id: DISCORD_ID, username: 'discord-user', global_name: null, avatar: null } }) }).fetchDiscord, now: () => NOW_MS });
    profile = await usernameFallback.getMemberProfile(config, DISCORD_ID);
    assert.equal(profile.display_name, 'discord-user');
    assert.equal(profile.global_display_name, 'discord-user');
    assert.equal(profile.pfp_url, defaultAvatarUrl(DISCORD_ID));
    const noRoles = new DiscordMemberService({ fetchDiscord: createFakeDiscord({ member: fakeMember({ roles: [] }) }).fetchDiscord, now: () => NOW_MS });
    profile = await noRoles.getMemberProfile(config, DISCORD_ID);
    assert.deepEqual(profile.roles, []);
    assert.equal(profile.highest_role, null);

    for (const mode of [
      { memberError: response(429, { retry_after: 1 }, { 'retry-after': '3' }), expected: 429 },
      { memberError: response(500, { message: 'server error' }), expected: 503 },
      { memberError: response(200, { user: { id: '111111111111111111', username: 'bad' }, roles: [] }), expected: 503 },
      { rolesError: response(200, { not: 'an array' }), expected: 503 }
    ]) {
      const failureRoute = await startRoute({
        loadDatabase: testDatabase,
        getConfig: () => config,
        discordService: new DiscordMemberService({ fetchDiscord: createFakeDiscord(mode).fetchDiscord, now: () => NOW_MS }),
        now: () => NOW_MS,
        rateLimiter: createRateLimiter({ now: () => NOW_MS, ipLimit: 1000, licenseLimit: 1000 }),
        replayCache: createReplayCache({ now: () => NOW_MS })
      });
      try {
        result = await expectStatus(failureRoute.port, makeRequest(), mode.expected);
        assert.equal(result.body.d, undefined);
        if (mode.expected === 429) assert.equal(result.headers['retry-after'], '3');
      } finally {
        await failureRoute.close();
      }
    }
    const timeoutRoute = await startRoute({
      loadDatabase: testDatabase,
      getConfig: () => config,
      discordService: new DiscordMemberService({ fetchDiscord: async () => { throw new DiscordApiError('timeout'); }, now: () => NOW_MS }),
      now: () => NOW_MS,
      rateLimiter: createRateLimiter({ now: () => NOW_MS, ipLimit: 1000, licenseLimit: 1000 }),
      replayCache: createReplayCache({ now: () => NOW_MS })
    });
    try {
      result = await expectStatus(timeoutRoute.port, makeRequest(), 503);
      assert.equal(result.body.d, undefined);
    } finally {
      await timeoutRoute.close();
    }

    const signatureRequest = makeRequest();
    result = await expectStatus(route.port, signatureRequest, 200);
    const tokenParts = decryptAttestation(result, signatureRequest).split('.');
    const changedPayload = [...tokenParts];
    changedPayload[1] = `${changedPayload[1].slice(0, -1)}${changedPayload[1].endsWith('A') ? 'B' : 'A'}`;
    await expectReject(() => verifyDiscordAttestation(changedPayload.join('.'), publicKeyPem, { issuer: ISSUER, audience: AUDIENCE, kid: KID }, NOW_UNIX));
    const changedSignature = [...tokenParts];
    changedSignature[2] = `${changedSignature[2].startsWith('A') ? 'B' : 'A'}${changedSignature[2].slice(1)}`;
    await expectReject(() => verifyDiscordAttestation(changedSignature.join('.'), publicKeyPem, { issuer: ISSUER, audience: AUDIENCE, kid: KID }, NOW_UNIX));
    const changedHeader = [...tokenParts];
    const header = JSON.parse(Buffer.from(changedHeader[0], 'base64url').toString('utf8'));
    header.alg = 'none';
    changedHeader[0] = Buffer.from(JSON.stringify(header)).toString('base64url');
    await expectReject(() => verifyDiscordAttestation(changedHeader.join('.'), publicKeyPem, { issuer: ISSUER, audience: AUDIENCE, kid: KID }, NOW_UNIX));

    const jose = await import('jose');
    const signingKey = await jose.importPKCS8(fs.readFileSync(privateKeyFile, 'utf8'), 'ES256');
    const baseClaims = { ...(await verifyDiscordAttestation(tokenParts.join('.'), publicKeyPem, { issuer: ISSUER, audience: AUDIENCE, kid: KID }, NOW_UNIX)).payload };
    async function signedVariant(claims, protectedHeader = {}) {
      return new jose.SignJWT(claims)
        .setProtectedHeader({ alg: 'ES256', kid: KID, typ: 'discord-member-info+jwt', ...protectedHeader })
        .setIssuer(claims.iss || ISSUER)
        .setAudience(claims.aud || AUDIENCE)
        .setSubject(claims.sub || `discord:${DISCORD_ID}`)
        .setIssuedAt(claims.iat || NOW_UNIX)
        .setNotBefore(claims.nbf || NOW_UNIX - 5)
        .setExpirationTime(claims.exp || NOW_UNIX + 60)
        .setJti('test-jti')
        .sign(signingKey);
    }
    for (const [claims, headerOverride, expected] of [
      [baseClaims, { typ: 'wrong' }, { issuer: ISSUER, audience: AUDIENCE, kid: KID }],
      [baseClaims, { kid: 'wrong' }, { issuer: ISSUER, audience: AUDIENCE, kid: KID }],
      [{ ...baseClaims, iss: 'wrong' }, {}, { issuer: ISSUER, audience: AUDIENCE, kid: KID }],
      [{ ...baseClaims, aud: 'wrong' }, {}, { issuer: ISSUER, audience: AUDIENCE, kid: KID }],
      [{ ...baseClaims, exp: NOW_UNIX - 1 }, {}, { issuer: ISSUER, audience: AUDIENCE, kid: KID }],
      [{ ...baseClaims, nbf: NOW_UNIX + 1 }, {}, { issuer: ISSUER, audience: AUDIENCE, kid: KID }]
    ]) {
      await expectReject(() => signedVariant(claims, headerOverride).then((token) => verifyDiscordAttestation(token, publicKeyPem, expected, NOW_UNIX)));
    }
    for (const field of ['nonce', 'discord_id', 'license_hash', 'hwid_hash', 'request_hash']) {
      const variant = await signedVariant({ ...baseClaims, [field]: 'changed' });
      await expectReject(() => verifyDiscordAttestation(variant, publicKeyPem, {
        issuer: ISSUER,
        audience: AUDIENCE,
        kid: KID,
        claims: { [field]: baseClaims[field] }
      }, NOW_UNIX));
    }

    const limited = await startRoute({
      loadDatabase: testDatabase,
      getConfig: () => config,
      discordService: new DiscordMemberService({ fetchDiscord: fake.fetchDiscord, now: () => NOW_MS }),
      now: () => NOW_MS,
      rateLimiter: createRateLimiter({ now: () => NOW_MS, ipLimit: 1, licenseLimit: 1, maxEntries: 2 }),
      replayCache: createReplayCache({ now: () => NOW_MS })
    });
    try {
      await expectStatus(limited.port, makeRequest(), 200);
      result = await expectStatus(limited.port, makeRequest(), 429);
      assert.equal(result.headers['retry-after'], '60');
    } finally {
      await limited.close();
    }

    console.log('Discord member-info tests passed.');
  } finally {
    await route.close();
    fs.rmSync(keyDirectory, { recursive: true, force: true });
  }
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
