const assert = require('assert/strict');
const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { Readable } = require('stream');
const test = require('node:test');

const { createStore } = require('../src/lib/data-store');
const { bindPlayerToLicense } = require('../src/player-service');
const { commandDefinitions, handleCommand, handleModuleModal, issuePairingCode, startDiscordPresence, validateDiscordApplicationConfiguration, verifyInteraction } = require('../src/discord/bot');
const { createModuleFileManager } = require('../src/lib/module-files');

const USER_ID = '111111111111111111';
const ADMIN_ID = '222222222222222222';

function interaction(name, options = [], admin = false, userId = USER_ID) {
  return {
    data: { name, options },
    member: {
      permissions: admin ? '8' : '0', roles: [],
      user: { id: userId, username: `user-${userId.slice(-4)}`, global_name: 'Test User', avatar: null }
    }
  };
}

function dmInteraction(name, options = [], userId = USER_ID) {
  return { data: { name, options }, user: { id: userId, username: `user-${userId.slice(-4)}`, global_name: 'DM User', avatar: null } };
}

function content(result) { return result.data.content; }
function option(name, value) { return { name, value }; }
function portableExecutable(marker = 0) {
  const buffer = Buffer.alloc(128, marker);
  buffer[0] = 0x4d; buffer[1] = 0x5a;
  buffer.writeUInt32LE(64, 0x3c); buffer.writeUInt32LE(0x00004550, 64);
  return buffer;
}
function moduleModalInteraction(action, filename = '', attachment = null, admin = true) {
  const result = interaction('unused', [], admin, admin ? ADMIN_ID : USER_ID);
  result.type = 5;
  result.data = {
    custom_id: 'modules:manager:v1',
    components: [
      { type: 18, component: { type: 3, custom_id: 'module_action', values: [action] } },
      { type: 18, component: { type: 4, custom_id: 'module_filename', value: filename } },
      { type: 18, component: { type: 19, custom_id: 'module_upload', values: attachment ? ['upload-1'] : [] } }
    ],
    resolved: { attachments: attachment ? { 'upload-1': attachment } : {} }
  };
  return result;
}

test('Discord Gateway presence safely stays disabled without a bot token', async () => {
  const previous = process.env.CRISIS_DISCORD_BOT_TOKEN;
  try {
    delete process.env.CRISIS_DISCORD_BOT_TOKEN;
    assert.equal(await startDiscordPresence(), null);
  } finally {
    if (previous === undefined) delete process.env.CRISIS_DISCORD_BOT_TOKEN;
    else process.env.CRISIS_DISCORD_BOT_TOKEN = previous;
  }
});

test('Discord startup preflight catches mismatched application credentials and endpoints', () => {
  const appId = '123456789012345678';
  const publicKey = 'ab'.repeat(32);
  const valid = { id: appId, name: 'Helper', verify_key: publicKey, interactions_endpoint_url: 'https://wauth.aaddpp.lol/api/discord/interactions' };
  assert.doesNotThrow(() => validateDiscordApplicationConfiguration(valid, appId, publicKey));
  assert.throws(() => validateDiscordApplicationConfiguration({ ...valid, verify_key: 'cd'.repeat(32) }, appId, publicKey), /PUBLIC_KEY/);
  assert.throws(() => validateDiscordApplicationConfiguration({ ...valid, interactions_endpoint_url: 'https://wauth.aaddpp.lol/wrong' }, appId, publicKey), /Endpoint URL/);
});

test('Discord pairing, status, time, and HWID approval workflows are authorized and audited', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-discord-bot-'));
  try {
    const store = createStore(path.join(directory, 'database.json'));
    store.initialize();
    const now = Math.floor(Date.now() / 1000);
    store.transaction((db) => {
      db.keys['LICENSE-PAIR-001'] = { duration_seconds: 86400, games: ['fortnite', 'rust'], start_unix: now, expiry_unix: now + 86400, hwid: 'BOUND-HWID', client_public_key: 'bound-device-key', created_at: now };
    });

    let pairing;
    store.transaction((db) => { pairing = issuePairingCode(db, 'LICENSE-PAIR-001'); });
    assert.match(pairing.code, /^[A-Z2-9]{4}-[A-Z2-9]{4}-[A-Z2-9]{4}$/);
    let repeatedPairing;
    store.transaction((db) => { repeatedPairing = issuePairingCode(db, 'LICENSE-PAIR-001'); });
    assert.equal(repeatedPairing.code, pairing.code, 'an unlinked license keeps one pairing code across launches');

    const services = { store, bindPlayerToLicense };
    const paired = handleCommand(interaction('pair', [option('code', pairing.code)]), services);
    assert.match(content(paired), /Linked license/);
    assert.equal(store.read().keys['LICENSE-PAIR-001'].discord_id, USER_ID);

    const replay = handleCommand(interaction('pair', [option('code', pairing.code)]), services);
    assert.match(content(replay), /invalid or expired/);

    const status = handleCommand(interaction('key-status'), services);
    assert.match(content(status), /LICE…-001/);
    assert.doesNotMatch(content(status), /LICENSE-PAIR-001/);
    assert.match(content(status), /HWID bound/);
    assert.match(content(status), /Fortnite \+ Rust/);

    const request = handleCommand(interaction('hwid-reset-request', [option('reason', 'Replaced motherboard')]), services);
    const requestId = content(request).match(/[A-F0-9]{12}/)?.[0];
    assert.ok(requestId);

    const deniedAdminAction = handleCommand(interaction('admin-time', [option('user', USER_ID), option('days', 2), option('reason', 'Support extension')]), services);
    assert.match(content(deniedAdminAction), /Administrator permission/);

    const previousExpiry = store.read().keys['LICENSE-PAIR-001'].expiry_unix;
    const adjusted = handleCommand(interaction('admin-time', [option('user', USER_ID), option('days', 2), option('reason', 'Support extension')], true, ADMIN_ID), services);
    assert.match(content(adjusted), /Updated 1 license/);
    assert.equal(store.read().keys['LICENSE-PAIR-001'].expiry_unix, previousExpiry + 2 * 86400);

    const approved = handleCommand(interaction('admin-reset-review', [option('request', requestId), option('decision', 'approve'), option('reason', 'Verified replacement')], true, ADMIN_ID), services);
    assert.match(content(approved), /reset 1 HWID/);
    const database = store.read();
    assert.equal(database.keys['LICENSE-PAIR-001'].hwid, null);
    assert.equal(database.keys['LICENSE-PAIR-001'].client_public_key, null);
    assert.equal(database.hwid_reset_requests[requestId].status, 'approved');
    assert.ok(database.audit_events.some((entry) => entry.action === 'discord.pair'));
    assert.ok(database.audit_events.some((entry) => entry.action === 'hwid_reset.approved'));
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

test('Discord interaction verification rejects tampering and stale requests', () => {
  const { publicKey, privateKey } = crypto.generateKeyPairSync('ed25519');
  const rawPublicKey = publicKey.export({ format: 'der', type: 'spki' }).subarray(-32).toString('hex');
  const body = Buffer.from(JSON.stringify({ type: 1 }));
  const timestamp = String(Math.floor(Date.now() / 1000));
  const signature = crypto.sign(null, Buffer.concat([Buffer.from(timestamp), body]), privateKey).toString('hex');
  const previous = process.env.CRISIS_DISCORD_PUBLIC_KEY;
  process.env.CRISIS_DISCORD_PUBLIC_KEY = rawPublicKey;
  try {
    const request = { body, get: (name) => name === 'x-signature-ed25519' ? signature : timestamp };
    assert.equal(verifyInteraction(request), true);
    assert.equal(verifyInteraction({ ...request, body: Buffer.from('{}') }), false);
    const stale = String(Math.floor(Date.now() / 1000) - 301);
    const staleSignature = crypto.sign(null, Buffer.concat([Buffer.from(stale), body]), privateKey).toString('hex');
    assert.equal(verifyInteraction({ body, get: (name) => name === 'x-signature-ed25519' ? staleSignature : stale }), false);
  } finally {
    if (previous === undefined) delete process.env.CRISIS_DISCORD_PUBLIC_KEY;
    else process.env.CRISIS_DISCORD_PUBLIC_KEY = previous;
  }
});

test('requested key-management commands generate and compose pause controls', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-discord-admin-'));
  try {
    const store = createStore(path.join(directory, 'database.json'));
    store.initialize();
    const services = { store, bindPlayerToLicense };
    const generated = handleCommand(interaction('generate', [option('length', 2), option('unit', 'days'), option('game', 'fortnite')], true, ADMIN_ID), services);
    const key = content(generated).match(/CRISIS-[A-Z2-9-]+/)?.[0];
    assert.ok(key);
    assert.equal(store.read().keys[key].duration_seconds, 2 * 86400);
    assert.deepEqual(store.read().keys[key].games, ['fortnite']);
    assert.match(content(generated), /for Fortnite/);
    assert.match(content(handleCommand(interaction('list', [option('game', 'fortnite')], true, ADMIN_ID), services)), /CRIS…[A-Z2-9]{4}/);
    assert.match(content(handleCommand(interaction('list', [option('game', 'rust')], true, ADMIN_ID), services)), /No license keys match/);

    assert.match(content(handleCommand(interaction('pause_key', [option('key', key)], true, ADMIN_ID), services)), /Paused/);
    assert.deepEqual(store.read().keys[key].pause_reasons, ['key-admin']);
    assert.match(content(handleCommand(interaction('pause_keys', [], true, ADMIN_ID), services)), /Global freeze enabled/);
    assert.deepEqual(new Set(store.read().keys[key].pause_reasons), new Set(['key-admin', 'global']));
    assert.match(content(handleCommand(interaction('resume_keys', [], true, ADMIN_ID), services)), /Global freeze removed/);
    assert.deepEqual(store.read().keys[key].pause_reasons, ['key-admin'], 'global resume does not bypass a key pause');
    assert.match(content(handleCommand(interaction('resume_key', [option('key', key)], true, ADMIN_ID), services)), /Resumed/);
    assert.deepEqual(store.read().keys[key].pause_reasons, []);

    const commands = commandDefinitions();
    const names = new Set(commands.map((command) => command.name));
    for (const name of ['generate', 'list', 'reset_user', 'reset_key', 'pause_keys', 'resume_keys', 'pause_key', 'resume_key', 'pause_user', 'resume_user', 'link', 'request_hwid_reset', 'time_left', 'pause_subscription', 'unpause_subscription', 'pair', 'modules']) assert.ok(names.has(name), name);
    for (const name of ['create_file', 'replace_file', 'delete_file']) assert.equal(names.has(name), false);
    const gameChoices = commands.find((command) => command.name === 'generate').options.find((entry) => entry.name === 'game').choices;
    assert.deepEqual(gameChoices.map((entry) => entry.value), ['fortnite', 'rust', 'all']);
    for (const command of commands) {
      assert.deepEqual(command.integration_types, [0, 1]);
      assert.deepEqual(command.contexts, [0, 1, 2]);
    }
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

test('module manager modal is admin-only and handles its complete file lifecycle', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-discord-modules-'));
  try {
    const store = createStore(path.join(directory, 'database.json'));
    store.initialize();
    const moduleFiles = createModuleFileManager({ storageRoot: path.join(directory, 'artifacts'), maxUploadBytes: 1024 });
    const services = { store, bindPlayerToLicense, moduleFiles };
    assert.match(content(handleCommand(interaction('modules'), services)), /Administrator permission/);
    const modal = handleCommand(interaction('modules', [], true, ADMIN_ID), services);
    assert.equal(modal.type, 9);
    assert.deepEqual(modal.data.components[0].component.options.map((entry) => entry.label), ['List', 'Create File', 'Replace / Update File', 'Delete File']);

    const first = portableExecutable(1);
    const second = portableExecutable(2);
    const attachment = (buffer, filename = 'feature.dll') => ({ filename, size: buffer.length, url: 'https://cdn.discordapp.com/attachments/test/file.dll' });
    const fetchBuffer = (buffer) => async () => ({ ok: true, status: 200, body: Readable.from([buffer]), headers: new Headers() });

    const denied = await handleModuleModal(moduleModalInteraction('list', '', null, false), services);
    assert.match(content(denied), /Administrator permission/);
    const created = await handleModuleModal(moduleModalInteraction('create', '', attachment(first)), { ...services, fetchImpl: fetchBuffer(first) });
    assert.match(content(created), /Created `crisis-feature.dll`/);
    assert.match(content(await handleModuleModal(moduleModalInteraction('list'), services)), /crisis-feature\.dll` — Universal/);
    const replaced = await handleModuleModal(moduleModalInteraction('replace', 'crisis-feature', attachment(second)), { ...services, fetchImpl: fetchBuffer(second) });
    assert.match(content(replaced), /Replaced `crisis-feature.dll`/);
    assert.deepEqual(fs.readFileSync(path.join(moduleFiles.currentDirectory, 'crisis-feature.dll')), second);
    const deleted = await handleModuleModal(moduleModalInteraction('delete', 'feature'), services);
    assert.match(content(deleted), /Deleted `crisis-feature.dll`/);
    assert.equal(moduleFiles.list().length, 0);
    assert.ok(store.read().audit_events.some((entry) => entry.action === 'file.create'));
    assert.ok(store.read().audit_events.some((entry) => entry.action === 'file.replace'));
    assert.ok(store.read().audit_events.some((entry) => entry.action === 'file.delete'));
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

test('configured administrator user IDs authorize commands outside a guild', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-discord-dm-admin-'));
  const previous = process.env.CRISIS_DISCORD_ADMIN_USER_IDS;
  try {
    const store = createStore(path.join(directory, 'database.json'));
    store.initialize();
    const services = { store, bindPlayerToLicense };
    process.env.CRISIS_DISCORD_ADMIN_USER_IDS = ADMIN_ID;
    const allowed = handleCommand(dmInteraction('generate', [option('length', 1), option('unit', 'hours'), option('game', 'rust')], ADMIN_ID), services);
    assert.match(content(allowed), /Generated 1 hours/);

    const denied = handleCommand(dmInteraction('generate', [option('length', 1), option('unit', 'hours'), option('game', 'rust')], USER_ID), services);
    assert.match(content(denied), /Administrator permission/);
  } finally {
    if (previous === undefined) delete process.env.CRISIS_DISCORD_ADMIN_USER_IDS;
    else process.env.CRISIS_DISCORD_ADMIN_USER_IDS = previous;
    fs.rmSync(directory, { recursive: true, force: true });
  }
});
