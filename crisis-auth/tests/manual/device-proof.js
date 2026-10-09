const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { artifactCanonical, authCanonical } = require('../../src/lib/client-proof');

function createIdentity() {
  const filename = process.env.CRISIS_TEST_DEVICE_KEY_FILE || path.join(os.homedir(), '.crisis-loader-test-device-key.pem');
  let privateKey;
  if (fs.existsSync(filename)) {
    privateKey = crypto.createPrivateKey(fs.readFileSync(filename));
  } else {
    privateKey = crypto.generateKeyPairSync('ec', { namedCurve: 'prime256v1' }).privateKey;
    fs.writeFileSync(filename, privateKey.export({ format: 'pem', type: 'pkcs8' }), { mode: 0o600, flag: 'wx' });
  }
  const pair = { privateKey, publicKey: crypto.createPublicKey(privateKey) };
  const jwk = pair.publicKey.export({ format: 'jwk' });
  const blob = Buffer.alloc(72);
  blob.writeUInt32LE(0x31534345, 0);
  blob.writeUInt32LE(32, 4);
  Buffer.from(jwk.x, 'base64url').copy(blob, 8);
  Buffer.from(jwk.y, 'base64url').copy(blob, 40);
  return { pair, publicKey: blob.toString('base64url') };
}

function sign(identity, canonical) {
  return crypto.sign('sha256', Buffer.from(canonical, 'utf8'), {
    key: identity.pair.privateKey,
    dsaEncoding: 'ieee-p1363'
  }).toString('base64url');
}

function signedAuthentication(identity, key, hwid) {
  const timestamp_unix = Math.floor(Date.now() / 1000);
  const nonce = crypto.randomBytes(32).toString('base64url');
  return {
    op: 'authenticate',
    key,
    hwid,
    client_public_key: identity.publicKey,
    timestamp_unix,
    nonce,
    signature: sign(identity, authCanonical(key, hwid, timestamp_unix, nonce))
  };
}

function artifactRequest(identity, op, name, accessToken) {
  const timestamp_unix = Math.floor(Date.now() / 1000);
  const nonce = crypto.randomBytes(32).toString('base64url');
  return {
    op,
    name,
    access_token: accessToken,
    timestamp_unix,
    nonce,
    signature: sign(identity, artifactCanonical(op, name, accessToken, timestamp_unix, nonce))
  };
}

module.exports = { artifactRequest, createIdentity, signedAuthentication };
