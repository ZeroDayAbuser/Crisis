const crypto = require('crypto');
const fs = require('fs');

async function loadJose() {
  return import('jose');
}

async function signDiscordAttestation(claims, config, nowUnix) {
  const { importPKCS8, SignJWT } = await loadJose();
  const privateKeyPem = fs.readFileSync(config.signingPrivateKeyFile, 'utf8');
  const privateKey = await importPKCS8(privateKeyPem, 'ES256');
  const iat = nowUnix;

  return new SignJWT(claims)
    .setProtectedHeader({ alg: 'ES256', kid: config.signingKeyId, typ: 'discord-member-info+jwt' })
    .setIssuer(config.signingIssuer)
    .setAudience(config.signingAudience)
    .setSubject(`discord:${claims.discord_id}`)
    .setIssuedAt(iat)
    .setNotBefore(iat - 5)
    .setExpirationTime(iat + 60)
    .setJti(crypto.randomUUID())
    .sign(privateKey);
}

async function verifyDiscordAttestation(token, publicKeyPem, expected, nowUnix) {
  const { importSPKI, jwtVerify } = await loadJose();
  const publicKey = await importSPKI(publicKeyPem, 'ES256');
  const result = await jwtVerify(token, publicKey, {
    algorithms: ['ES256'],
    issuer: expected.issuer,
    audience: expected.audience,
    currentDate: new Date(nowUnix * 1000)
  });

  if (result.protectedHeader.typ !== 'discord-member-info+jwt' || result.protectedHeader.kid !== expected.kid) {
    throw new Error('Unexpected attestation header');
  }
  if (expected.claims) {
    for (const [name, value] of Object.entries(expected.claims)) {
      if (result.payload[name] !== value) throw new Error(`Unexpected attestation claim: ${name}`);
    }
  }
  return result;
}

module.exports = {
  signDiscordAttestation,
  verifyDiscordAttestation
};
