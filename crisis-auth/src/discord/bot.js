const crypto = require('crypto');
const express = require('express');
const {
  isLicensePaused, pauseAll, pauseLicense, reasons,
  remainingSeconds, resumeAll, resumeLicense
} = require('../lib/license-state');
const {
  GAMES, GAME_IDS, formatGames, gameChoices, licenseAllowsGame, parseGameSelection, requiredGameForModule
} = require('../lib/game-entitlements');
const { ModuleFileError, normalizeModuleName } = require('../lib/module-files');
const {
  MAX_ORDER_QUANTITY, PRODUCTS, addReseller, adjustResellerBalance,
  centsFromAmount, formatMoney, generateResellerOrder, resellerAccount
} = require('../reseller-service');

const PAIRING_CODE_LENGTH = 12;
const RESET_REQUEST_SECONDS = 7 * 24 * 60 * 60;
const CODE_ALPHABET = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
const EPHEMERAL = 1 << 6;
const ED25519_SPKI_PREFIX = Buffer.from('302a300506032b6570032100', 'hex');
const MODULE_MODAL_ID = 'modules:manager:v1';
const RESELLER_MODAL_ID = 'reseller:manager:v1';
let presenceClient = null;

function nowUnix() { return Math.floor(Date.now() / 1000); }
function hash(domain, value) { return crypto.createHash('sha256').update(domain).update('\0').update(value).digest('base64url'); }
function licenseDigest(key) { return hash('crisis-discord-license:v1', key); }
function pairingDigest(code) { return hash('crisis-discord-pairing:v1', normalizeCode(code)); }
function normalizeCode(code) { return String(code || '').trim().toUpperCase().replace(/[^A-Z0-9]/g, ''); }
function randomCharacters(length) {
  let value = '';
  for (let index = 0; index < length; index++) value += CODE_ALPHABET[crypto.randomInt(CODE_ALPHABET.length)];
  return value;
}
function randomCode() {
  const value = randomCharacters(PAIRING_CODE_LENGTH);
  return `${value.slice(0, 4)}-${value.slice(4, 8)}-${value.slice(8)}`;
}
function randomLicenseKey() {
  const value = randomCharacters(20);
  return `CRISIS-${value.slice(0, 5)}-${value.slice(5, 10)}-${value.slice(10, 15)}-${value.slice(15)}`;
}
function publicRequestId() { return crypto.randomBytes(6).toString('hex').toUpperCase(); }
function maskKey(key) { return key.length <= 8 ? `${key.slice(0, 2)}…${key.slice(-2)}` : `${key.slice(0, 4)}…${key.slice(-4)}`; }
function formatDuration(seconds) {
  if (seconds <= 0) return 'expired';
  const days = Math.floor(seconds / 86400);
  const hours = Math.floor((seconds % 86400) / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  return [days && `${days}d`, hours && `${hours}h`, (!days && minutes) && `${minutes}m`].filter(Boolean).join(' ') || '<1m';
}
function discordProfile(user) {
  const avatar = user.avatar ? `https://cdn.discordapp.com/avatars/${user.id}/${user.avatar}.png` : '';
  return { username: user.username || '', display_name: user.global_name || user.username || '', avatar_url: avatar };
}
function audit(db, actorId, target, action, reason, previousValue, newValue) {
  if (!Array.isArray(db.audit_events)) db.audit_events = [];
  db.audit_events.push({
    id: crypto.randomUUID(), actor_user_id: `discord:${actorId}`, actor_role: 'discord_bot', target, action, reason,
    previous_value: previousValue, new_value: newValue, created_at: nowUnix(), request_id: crypto.randomUUID(), metadata: { source: 'discord' }
  });
}
function linkedLicenses(db, discordId) { return Object.entries(db.keys || {}).filter(([, license]) => license.discord_id === discordId); }
function findLicenseKey(db, digest) { return Object.keys(db.keys || {}).find((key) => licenseDigest(key) === digest) || null; }

function issuePairingCode(db, key, current = nowUnix()) {
  if (!db.discord_pairing_codes || typeof db.discord_pairing_codes !== 'object') db.discord_pairing_codes = {};
  for (const [id, record] of Object.entries(db.discord_pairing_codes)) {
    if (record.expires_at < current || record.consumed_at) delete db.discord_pairing_codes[id];
  }
  const license = db.keys?.[key];
  if (!license || license.discord_id || !Number.isSafeInteger(license.expiry_unix) || license.expiry_unix <= current) return null;
  const digest = licenseDigest(key);
  const existing = Object.values(db.discord_pairing_codes).find((record) =>
    record.license_digest === digest && !record.consumed_at && record.expires_at >= current &&
    typeof record.code === 'string' && pairingDigest(record.code) === record.code_digest);
  if (existing) return { code: existing.code, expires_in: existing.expires_at - current };
  for (const [id, record] of Object.entries(db.discord_pairing_codes)) {
    if (record.license_digest === digest) delete db.discord_pairing_codes[id];
  }
  let code;
  let codeDigest;
  do { code = randomCode(); codeDigest = pairingDigest(code); }
  while (Object.values(db.discord_pairing_codes).some((record) => record.code_digest === codeDigest));
  const id = crypto.randomUUID();
  db.discord_pairing_codes[id] = {
    id, code, code_digest: codeDigest, license_digest: digest, created_at: current,
    expires_at: license.expiry_unix, consumed_at: null, discord_id: null
  };
  return { code, expires_in: license.expiry_unix - current };
}

function option(interaction, name) { return interaction.data?.options?.find((entry) => entry.name === name)?.value; }
function actor(interaction) { return interaction.member?.user || interaction.user || {}; }
function response(content) { return { type: 4, data: { content: String(content).slice(0, 1900), flags: EPHEMERAL, allowed_mentions: { parse: [] } } }; }
function responseWithFile(content, filename, buffer) {
  return { ...response(content), file: { filename, buffer } };
}
function resellerModal() {
  return {
    type: 9,
    data: {
      custom_id: RESELLER_MODAL_ID,
      title: 'Reseller Account',
      components: [
        { type: 18, label: 'Action', component: { type: 3, custom_id: 'reseller_action', min_values: 1, max_values: 1, options: [
          { label: 'View balance and totals', value: 'balance' },
          { label: 'Generate license keys', value: 'generate' }
        ] } },
        { type: 18, label: 'Key duration', description: 'Required when generating keys', component: { type: 3, custom_id: 'reseller_product', min_values: 0, max_values: 1, options: Object.entries(PRODUCTS).map(([value, product]) => ({ label: product.label, value })) } },
        { type: 18, label: 'Games', description: 'Every generated key receives the selected game access', component: { type: 3, custom_id: 'reseller_games', min_values: 1, max_values: GAME_IDS.length, options: gameChoices(false).map(({ name: label, value }) => ({ label, value })) } },
        { type: 18, label: 'Quantity', description: `Required when generating; maximum ${MAX_ORDER_QUANTITY}`, component: { type: 4, custom_id: 'reseller_quantity', style: 1, required: false, min_length: 1, max_length: 4, placeholder: '1' } }
      ]
    }
  };
}
function modulesModal() {
  return {
    type: 9,
    data: {
      custom_id: MODULE_MODAL_ID,
      title: 'Module File Management',
      components: [
        { type: 18, label: 'Action', component: { type: 3, custom_id: 'module_action', min_values: 1, max_values: 1, options: [
          { label: 'List', value: 'list' },
          { label: 'Create File', value: 'create' },
          { label: 'Replace / Update File', value: 'replace' },
          { label: 'Delete File', value: 'delete' }
        ] } },
        { type: 18, label: 'Filename', description: 'Required for delete; optional when uploading', component: { type: 4, custom_id: 'module_filename', style: 1, required: false, max_length: 120, placeholder: 'memory or crisis-memory' } },
        { type: 18, label: 'Module file', description: 'Required for create and replace', component: { type: 19, custom_id: 'module_upload', min_values: 0, max_values: 1, required: false } }
      ]
    }
  };
}
function configuredAdminRoles() { return new Set(String(process.env.CRISIS_DISCORD_ADMIN_ROLE_IDS || '').split(',').map((value) => value.trim()).filter(Boolean)); }
function configuredAdminUsers() { return new Set(String(process.env.CRISIS_DISCORD_ADMIN_USER_IDS || '').split(',').map((value) => value.trim()).filter(Boolean)); }
function isAdmin(interaction) {
  const userId = actor(interaction).id;
  if (userId && configuredAdminUsers().has(userId)) return true;
  const roles = configuredAdminRoles();
  if (roles.size) return (interaction.member?.roles || []).some((role) => roles.has(role));
  try { return (BigInt(interaction.member?.permissions || '0') & 8n) === 8n; }
  catch { return false; }
}
function commandName(interaction) {
  const aliases = {
    'key-status': 'time_left', 'hwid-reset-request': 'request_hwid_reset', 'admin-user': 'admin_user',
    'admin-time': 'admin_time', 'admin-reset-requests': 'admin_reset_requests', 'admin-reset-review': 'admin_reset_review'
  };
  return aliases[interaction.data?.name] || interaction.data?.name;
}
function statusLines(db, discordId) {
  const licenses = linkedLicenses(db, discordId);
  return licenses.map(([key, license]) => {
    const paused = isLicensePaused(db, license);
    return `${maskKey(key)} — ${formatGames(license.games)} — ${formatDuration(remainingSeconds(license))} remaining — ${paused ? 'paused' : 'active'} — HWID ${license.hwid ? 'bound' : 'not bound'}`;
  });
}
function pairAccount(db, code, user, bindPlayerToLicense) {
  const normalized = normalizeCode(code);
  if (!new RegExp(`^[A-Z2-9]{${PAIRING_CODE_LENGTH}}$`).test(normalized)) return { error: 'Pairing code is invalid or expired.' };
  const record = Object.values(db.discord_pairing_codes || {}).find((entry) => entry.code_digest === pairingDigest(normalized));
  if (!record || record.consumed_at || record.expires_at < nowUnix()) return { error: 'Pairing code is invalid or expired.' };
  const key = findLicenseKey(db, record.license_digest);
  if (!key) return { error: 'Pairing code is invalid or expired.' };
  const linked = bindPlayerToLicense(db, key, user.id, discordProfile(user));
  if (linked.error) return { error: linked.error };
  record.consumed_at = nowUnix();
  record.discord_id = user.id;
  audit(db, user.id, `license:${record.license_digest}`, 'discord.pair', 'User completed one-time pairing', null, { discord_id: user.id });
  return { key };
}

function commandDefinitions() {
  const string = 3;
  const integer = 4;
  const user = 6;
  const number = 10;
  const keyOption = { type: string, name: 'key', description: 'License key', required: true, min_length: 8, max_length: 256 };
  const userOption = { type: user, name: 'user', description: 'Discord user', required: true };
  const pairCode = { type: string, name: 'code', description: 'Pairing code shown by the client', required: true, min_length: 12, max_length: 14 };
  const gameOption = { type: string, name: 'game', description: 'Game access for the license', required: true, choices: gameChoices() };
  return [
    { name: 'generate', description: 'Generate a new license key', options: [{ type: integer, name: 'length', description: 'Subscription length', required: true, min_value: 1, max_value: 87600 }, { type: string, name: 'unit', description: 'Time unit', required: true, choices: [{ name: 'Hours', value: 'hours' }, { name: 'Days', value: 'days' }, { name: 'Weeks', value: 'weeks' }] }, gameOption] },
    { name: 'list', description: 'List license keys by game', options: [{ ...gameOption, required: false, description: 'Only show keys with this game; omit for all keys' }] },
    { name: 'add_reseller', description: 'Create a reseller account', options: [userOption, { type: number, name: 'starting_balance', description: 'Starting balance in USD', required: false, min_value: 0, max_value: 1000000 }] },
    { name: 'reseller_balance', description: 'Add, set, or remove reseller balance', options: [userOption, { type: string, name: 'action', description: 'Balance operation', required: true, choices: [{ name: 'Add', value: 'add' }, { name: 'Set', value: 'set' }, { name: 'Remove', value: 'remove' }] }, { type: number, name: 'amount', description: 'Amount in USD', required: true, min_value: 0, max_value: 1000000 }, { type: string, name: 'reason', description: 'Audit reason', required: true, min_length: 3, max_length: 300 }] },
    { name: 'reseller', description: 'Open your reseller account manager' },
    { name: 'reset_user', description: 'Reset every HWID linked to a Discord user', options: [{ type: user, name: 'discord_user', description: 'Discord user', required: true }] },
    { name: 'reset_key', description: 'Reset one key’s HWID', options: [keyOption] },
    { name: 'pause_keys', description: 'Freeze all subscriptions' },
    { name: 'resume_keys', description: 'Resume all globally frozen subscriptions' },
    { name: 'pause_key', description: 'Freeze one key', options: [keyOption] },
    { name: 'resume_key', description: 'Resume one administrator-frozen key', options: [keyOption] },
    { name: 'pause_user', description: 'Freeze every key linked to a Discord user', options: [userOption] },
    { name: 'resume_user', description: 'Resume administrator-frozen keys for a Discord user', options: [userOption] },
    { name: 'link', description: 'Link your Discord using the code shown by the client', options: [pairCode] },
    { name: 'pair', description: 'Link your Discord using the code shown by the client', options: [pairCode] },
    { name: 'request_hwid_reset', description: 'Request an administrator-approved HWID reset', options: [{ type: string, name: 'reason', description: 'Optional reason', required: false, max_length: 300 }] },
    { name: 'time_left', description: 'Show your linked keys and remaining subscription time' },
    { name: 'pause_subscription', description: 'Freeze your subscription time' },
    { name: 'unpause_subscription', description: 'Resume your self-frozen subscription time' },
    { name: 'admin_user', description: 'Show a Discord user’s license status', options: [userOption] },
    { name: 'admin_time', description: 'Add or remove subscription days', options: [userOption, { type: integer, name: 'days', description: 'Positive or negative days', required: true, min_value: -3650, max_value: 3650 }, { type: string, name: 'reason', description: 'Audit reason', required: true, min_length: 3, max_length: 300 }] },
    { name: 'admin_reset_requests', description: 'List pending HWID reset requests' },
    { name: 'admin_reset_review', description: 'Approve or deny an HWID reset request', options: [{ type: string, name: 'request', description: 'Request ID', required: true }, { type: string, name: 'decision', description: 'Decision', required: true, choices: [{ name: 'Approve', value: 'approve' }, { name: 'Deny', value: 'deny' }] }, { type: string, name: 'reason', description: 'Audit reason', required: true, min_length: 3, max_length: 300 }] },
    { name: 'modules', description: 'Manage module files' }
  ].map((command) => ({ ...command, integration_types: [0, 1], contexts: [0, 1, 2] }));
}

function handleCommand(interaction, { store, bindPlayerToLicense }) {
  const user = actor(interaction);
  if (!/^\d{17,20}$/.test(user.id || '')) return response('Discord identity was unavailable.');
  const command = commandName(interaction);

  if (command === 'modules') return isAdmin(interaction) ? modulesModal() : response('Administrator permission is required.');
  if (command === 'reseller') {
    return resellerAccount(store.read(), user.id)
      ? resellerModal()
      : response('You do not have an active reseller account.');
  }


  if (command === 'pair' || command === 'link') {
    let outcome;
    store.transaction((db) => { outcome = pairAccount(db, option(interaction, 'code'), user, bindPlayerToLicense); });
    return outcome.error ? response(outcome.error) : response(`Linked license ${maskKey(outcome.key)} to your Discord account.`);
  }

  if (command === 'time_left' || command === 'admin_user') {
    if (command === 'admin_user' && !isAdmin(interaction)) return response('Administrator permission is required.');
    const discordId = command === 'admin_user' ? String(option(interaction, 'user')) : user.id;
    const lines = statusLines(store.read(), discordId);
    return response(lines.length ? lines.join('\n') : 'No license is linked to that Discord account.');
  }

  if (command === 'request_hwid_reset') {
    const reason = String(option(interaction, 'reason') || 'User requested an HWID reset through Discord').trim();
    if (reason.length > 300) return response('The reason must be 300 characters or fewer.');
    let requestId = null;
    store.transaction((db) => {
      if (!linkedLicenses(db, user.id).length) return;
      if (!db.hwid_reset_requests || typeof db.hwid_reset_requests !== 'object') db.hwid_reset_requests = {};
      const existing = Object.values(db.hwid_reset_requests).find((entry) => entry.discord_id === user.id && entry.status === 'pending' && entry.expires_at > nowUnix());
      if (existing) { requestId = existing.id; return; }
      do { requestId = publicRequestId(); } while (db.hwid_reset_requests[requestId]);
      db.hwid_reset_requests[requestId] = { id: requestId, discord_id: user.id, reason, status: 'pending', created_at: nowUnix(), expires_at: nowUnix() + RESET_REQUEST_SECONDS, reviewed_at: null, reviewed_by: null, review_reason: null };
      audit(db, user.id, `discord:${user.id}`, 'hwid_reset.request', reason, null, { request_id: requestId });
    });
    return requestId ? response(`HWID reset request submitted. Request ID: ${requestId}`) : response('Link a license before requesting an HWID reset.');
  }

  if (command === 'pause_subscription' || command === 'unpause_subscription') {
    let linked = 0;
    let changed = 0;
    store.transaction((db) => {
      const licenses = linkedLicenses(db, user.id);
      linked = licenses.length;
      for (const [, license] of licenses) {
        const result = command === 'pause_subscription' ? pauseLicense(license, 'user-self') : resumeLicense(license, 'user-self');
        if (result) changed += 1;
      }
      if (changed) audit(db, user.id, `discord:${user.id}`, `subscription.${command === 'pause_subscription' ? 'pause' : 'resume'}`, 'User self-service command', null, { licenses: changed });
    });
    if (!linked) return response('Link a license before managing your subscription.');
    if (!changed) return response(command === 'pause_subscription' ? 'Your subscription is already paused.' : 'Your subscription was not self-paused. An administrator or global pause may still apply.');
    return response(command === 'pause_subscription' ? `Paused ${changed} subscription(s). Time will not decrease.` : `Resumed ${changed} subscription(s).`);
  }

  if (!isAdmin(interaction)) return response('Administrator permission is required.');
  if (command === 'list') {
    const selected = option(interaction, 'game') || 'all';
    const games = parseGameSelection(selected);
    if (!games) return response('Choose a valid game.');
    const database = store.read();
    const licenses = Object.entries(database.keys || {})
      .filter(([, license]) => selected === 'all' || games.some((game) => licenseAllowsGame(license, game)));
    const lines = licenses.slice(0, 40).map(([key, license]) =>
      `${maskKey(key)} — ${formatGames(license.games)} — ${formatDuration(remainingSeconds(license))} — ${isLicensePaused(database, license) ? 'paused' : 'active'}`
    );
    if (licenses.length > 40) lines.push(`…and ${licenses.length - 40} more.`);
    return response(lines.length ? `**License keys (${licenses.length})**\n${lines.join('\n')}` : 'No license keys match that game.');
  }

  if (command === 'add_reseller') {
    const discordId = String(option(interaction, 'user') || '');
    const startingBalanceCents = centsFromAmount(option(interaction, 'starting_balance') ?? 0);
    if (startingBalanceCents === null) return response('Starting balance must be a USD amount with no more than two decimal places.');
    try {
      let account;
      store.transaction((db) => {
        account = addReseller(db, discordId, startingBalanceCents, user.id);
        audit(db, user.id, `reseller:${discordId}`, 'reseller.create', 'Discord administrator command', null, { balance_cents: startingBalanceCents });
      });
      return response(`Created reseller account for <@${discordId}> with ${formatMoney(account.balance_cents)} balance.`);
    } catch (error) {
      return response(error.message);
    }
  }

  if (command === 'reseller_balance') {
    const discordId = String(option(interaction, 'user') || '');
    const action = String(option(interaction, 'action') || '');
    const amountCents = centsFromAmount(option(interaction, 'amount'));
    const reason = String(option(interaction, 'reason') || '').trim();
    if (amountCents === null || reason.length < 3 || reason.length > 300) return response('Amount must be valid USD with at most two decimal places, and an audit reason is required.');
    try {
      let result;
      store.transaction((db) => {
        result = adjustResellerBalance(db, discordId, action, amountCents, user.id, reason);
        audit(db, user.id, `reseller:${discordId}`, `reseller.balance.${action}`, reason, { balance_cents: result.before }, { balance_cents: result.after });
      });
      return response(`${action === 'set' ? 'Set' : action === 'add' ? 'Added to' : 'Removed from'} <@${discordId}> reseller balance. New balance: ${formatMoney(result.after)}.`);
    } catch (error) {
      return response(error.message);
    }
  }


  if (command === 'generate') {
    const length = Number(option(interaction, 'length'));
    const unit = option(interaction, 'unit');
    const games = parseGameSelection(option(interaction, 'game'));
    const multiplier = { hours: 3600, days: 86400, weeks: 604800 }[unit];
    const duration = length * multiplier;
    if (!Number.isSafeInteger(length) || length < 1 || !multiplier || !Number.isSafeInteger(duration) || duration > 10 * 365 * 86400) return response('Choose a duration between one hour and ten years.');
    if (!games) return response('Choose a valid game.');
    let key;
    store.transaction((db) => {
      do { key = randomLicenseKey(); } while (db.keys[key]);
      db.keys[key] = { duration_seconds: duration, games: [...games], start_unix: null, expiry_unix: null, hwid: null, created_at: nowUnix(), pause_reasons: [], pause_started_at: null };
      if (db.service_state?.keys_paused) pauseLicense(db.keys[key], 'global');
      audit(db, user.id, `license:${licenseDigest(key)}`, 'license.create', `Generated for ${length} ${unit}`, null, { duration_seconds: duration, games: [...games] });
    });
    return response(`Generated ${length} ${unit} for ${formatGames(games)}:\n\`${key}\``);
  }

  if (command === 'reset_key' || command === 'pause_key' || command === 'resume_key') {
    const key = String(option(interaction, 'key') || '').trim();
    let outcome = 'License key was not found.';
    store.transaction((db) => {
      const license = db.keys?.[key];
      if (!license) return;
      if (command === 'reset_key') { const changed = Boolean(license.hwid || license.client_public_key); license.hwid = null; license.client_public_key = null; outcome = changed ? `Reset device binding for ${maskKey(key)}.` : `${maskKey(key)} has no device binding.`; }
      if (command === 'pause_key') outcome = pauseLicense(license, 'key-admin') ? `Paused ${maskKey(key)}.` : `${maskKey(key)} is already administrator-paused.`;
      if (command === 'resume_key') outcome = resumeLicense(license, 'key-admin') ? `Resumed ${maskKey(key)}.` : `${maskKey(key)} is not administrator-paused.`;
      audit(db, user.id, `license:${licenseDigest(key)}`, command, 'Discord administrator command', null, { changed: !outcome.includes('already') && !outcome.includes('not administrator') });
    });
    return response(outcome);
  }

  if (command === 'reset_user' || command === 'pause_user' || command === 'resume_user') {
    const discordId = String(option(interaction, command === 'reset_user' ? 'discord_user' : 'user') || '');
    if (!/^\d{17,20}$/.test(discordId)) return response('A valid Discord user is required.');
    let linked = 0;
    let changed = 0;
    store.transaction((db) => {
      const licenses = linkedLicenses(db, discordId);
      linked = licenses.length;
      for (const [, license] of licenses) {
        if (command === 'reset_user' && (license.hwid || license.client_public_key)) { license.hwid = null; license.client_public_key = null; changed += 1; }
        if (command === 'pause_user' && pauseLicense(license, `user-admin:${discordId}`)) changed += 1;
        if (command === 'resume_user' && resumeLicense(license, `user-admin:${discordId}`)) changed += 1;
      }
      if (linked) audit(db, user.id, `discord:${discordId}`, command, 'Discord administrator command', null, { licenses: changed });
    });
    return response(linked ? `${command.replaceAll('_', ' ')}: changed ${changed} of ${linked} linked license(s).` : 'No license is linked to that Discord account.');
  }

  if (command === 'pause_keys' || command === 'resume_keys') {
    let changed = 0;
    store.transaction((db) => {
      changed = command === 'pause_keys' ? pauseAll(db) : resumeAll(db);
      audit(db, user.id, 'licenses:all', command, 'Discord administrator command', null, { licenses: changed });
    });
    return response(command === 'pause_keys' ? `Global freeze enabled for ${changed} key(s).` : `Global freeze removed; resumed ${changed} key(s).`);
  }

  if (command === 'admin_time') {
    const discordId = String(option(interaction, 'user'));
    const days = Number(option(interaction, 'days'));
    const reason = String(option(interaction, 'reason') || '').trim();
    if (!/^\d{17,20}$/.test(discordId) || !Number.isSafeInteger(days) || days === 0 || Math.abs(days) > 3650 || reason.length < 3 || reason.length > 300) return response('A Discord user, non-zero adjustment within 3650 days, and audit reason are required.');
    let changed = 0;
    let latest = 0;
    store.transaction((db) => {
      const current = nowUnix();
      for (const [, license] of linkedLicenses(db, discordId)) {
        const active = Number.isSafeInteger(license.start_unix);
        const previous = active && Number.isSafeInteger(license.expiry_unix) ? license.expiry_unix : null;
        let next = null;
        if (active) { next = Math.max(current, (previous ?? current) + days * 86400); license.expiry_unix = next; latest = Math.max(latest, next); }
        else { const oldDuration = Number.isSafeInteger(license.duration_seconds) ? license.duration_seconds : 2592000; license.duration_seconds = Math.max(60, oldDuration + days * 86400); license.expiry_unix = null; latest = Math.max(latest, current + license.duration_seconds); }
        changed += 1;
        db.subscription_ledger.push({ id: crypto.randomUUID(), discord_id: discordId, seconds: days * 86400, previous, expiry: next, reason, actor_discord_id: user.id, created_at: current });
      }
      if (changed) audit(db, user.id, `discord:${discordId}`, 'subscription.adjust', reason, null, { days, licenses: changed, expiry: latest });
    });
    return response(changed ? `Updated ${changed} license(s). New maximum expiry: <t:${latest}:F>.` : 'No license is linked to that Discord account.');
  }

  if (command === 'admin_reset_requests') {
    const pending = Object.values(store.read().hwid_reset_requests || {}).filter((entry) => entry.status === 'pending' && entry.expires_at > nowUnix()).slice(0, 20);
    return response(pending.length ? pending.map((entry) => `${entry.id} — <@${entry.discord_id}> — ${entry.reason}`).join('\n') : 'There are no pending HWID reset requests.');
  }

  if (command === 'admin_reset_review') {
    const requestId = String(option(interaction, 'request') || '').trim().toUpperCase();
    const decision = option(interaction, 'decision');
    const reviewReason = String(option(interaction, 'reason') || '').trim();
    if (!['approve', 'deny'].includes(decision) || reviewReason.length < 3 || reviewReason.length > 300) return response('A valid decision and audit reason are required.');
    let outcome = 'Reset request was not found or is no longer pending.';
    store.transaction((db) => {
      const request = db.hwid_reset_requests?.[requestId];
      if (!request || request.status !== 'pending' || request.expires_at < nowUnix()) return;
      let resetCount = 0;
      if (decision === 'approve') for (const [, license] of linkedLicenses(db, request.discord_id)) if (license.hwid || license.client_public_key) { license.hwid = null; license.client_public_key = null; resetCount += 1; }
      request.status = decision === 'approve' ? 'approved' : 'denied';
      request.reviewed_at = nowUnix(); request.reviewed_by = user.id; request.review_reason = reviewReason;
      audit(db, user.id, `discord:${request.discord_id}`, `hwid_reset.${request.status}`, reviewReason, { request_id: requestId }, { reset_count: resetCount });
      outcome = decision === 'approve' ? `Approved ${requestId}; reset ${resetCount} HWID binding(s).` : `Denied ${requestId}.`;
    });
    return response(outcome);
  }

  return response('Unknown command.');
}

function modalComponent(data, customId) {
  const pending = [...(data?.components || [])];
  while (pending.length) {
    const component = pending.shift();
    if (component?.custom_id === customId) return component;
    if (component?.component) pending.push(component.component);
    if (Array.isArray(component?.components)) pending.push(...component.components);
  }
  return null;
}

function formatBytes(bytes) {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

function allowedAttachmentUrl(value) {
  try {
    const url = new URL(value);
    return url.protocol === 'https:' && ['cdn.discordapp.com', 'media.discordapp.net'].includes(url.hostname.toLowerCase()) ? url : null;
  } catch { return null; }
}

async function downloadDiscordAttachment(attachment, maximum, fetchImpl = fetch) {
  if (!attachment || !Number.isSafeInteger(attachment.size) || attachment.size < 1 || attachment.size > maximum) {
    throw new ModuleFileError(`The uploaded module must be no larger than ${formatBytes(maximum)}.`);
  }
  let url = allowedAttachmentUrl(attachment.url);
  if (!url) throw new ModuleFileError('Discord returned an invalid attachment URL.');
  for (let redirect = 0; redirect < 4; redirect++) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 15000);
    try {
      const result = await fetchImpl(url, { redirect: 'manual', signal: controller.signal });
      if ([301, 302, 303, 307, 308].includes(result.status)) {
        url = allowedAttachmentUrl(result.headers.get('location'));
        if (!url) throw new ModuleFileError('Discord redirected the attachment to an unsupported host.');
        continue;
      }
      if (!result.ok || !result.body) throw new ModuleFileError('Discord could not provide the uploaded file.');
      const chunks = [];
      let length = 0;
      for await (const chunk of result.body) {
        length += chunk.length;
        if (length > maximum) {
          controller.abort();
          throw new ModuleFileError(`The uploaded module must be no larger than ${formatBytes(maximum)}.`);
        }
        chunks.push(Buffer.from(chunk));
      }
      return Buffer.concat(chunks, length);
    } finally {
      clearTimeout(timeout);
    }
  }
  throw new ModuleFileError('Discord redirected the attachment too many times.');
}

async function handleModuleModal(interaction, { store, moduleFiles, fetchImpl = fetch }) {
  if (!isAdmin(interaction)) return response('Administrator permission is required.');
  if (!moduleFiles) return response('Module file management is unavailable.');
  const action = modalComponent(interaction.data, 'module_action')?.values?.[0];
  const filenameInput = String(modalComponent(interaction.data, 'module_filename')?.value || '').trim();
  if (!['list', 'create', 'replace', 'delete'].includes(action)) return response('Choose a valid module action.');

  try {
    if (action === 'list') {
      const files = moduleFiles.list();
      const lines = files.slice(0, 40).map((file) => {
        const game = requiredGameForModule(file.filename);
        return `\`${file.filename}\` — ${game ? GAMES[game].label : 'Universal'} — ${formatBytes(file.size)} — <t:${file.modified_at}:R>`;
      });
      if (files.length > 40) lines.push(`…and ${files.length - 40} more.`);
      return response(lines.length ? `**Module files (${files.length})**\n${lines.join('\n')}` : 'No module files are stored.');
    }

    if (action === 'delete') {
      const previous = moduleFiles.remove(filenameInput);
      store.transaction((db) => audit(db, actor(interaction).id, `file:${previous.filename}`, 'file.delete', 'Discord administrator module manager', previous, null));
      return response(`Deleted \`${previous.filename}\`. A rollback copy was retained.`);
    }

    const uploadIds = modalComponent(interaction.data, 'module_upload')?.values || [];
    if (uploadIds.length !== 1) return response(`Upload one DLL to ${action === 'create' ? 'create' : 'replace'} a module.`);
    const attachment = interaction.data?.resolved?.attachments?.[uploadIds[0]];
    if (!attachment) return response('Discord did not include the uploaded file.');
    const filename = normalizeModuleName(filenameInput || attachment.filename);
    const previous = action === 'replace' ? moduleFiles.list().find((file) => file.filename.toLowerCase() === filename.toLowerCase()) || null : null;
    const buffer = await downloadDiscordAttachment(attachment, moduleFiles.maxUploadBytes, fetchImpl);
    const next = moduleFiles.store(filename, buffer, action);
    store.transaction((db) => audit(db, actor(interaction).id, `file:${next.filename}`, `file.${action}`, 'Discord administrator module manager', previous, next));
    return response(`${action === 'create' ? 'Created' : 'Replaced'} \`${next.filename}\` (${formatBytes(next.size)}).${action === 'replace' ? ' A rollback copy was retained.' : ''}`);
  } catch (error) {
    if (!(error instanceof ModuleFileError)) console.error('[DISCORD] Module file operation failed:', error.message);
    return response(error instanceof ModuleFileError ? error.message : 'The module file operation failed.');
  }
}
async function handleResellerModal(interaction, { store }) {
  const user = actor(interaction);
  if (!/^\d{17,20}$/.test(user.id || '')) return response('Discord identity was unavailable.');
  const action = modalComponent(interaction.data, 'reseller_action')?.values?.[0];
  if (action === 'balance') {
    const account = resellerAccount(store.read(), user.id);
    if (!account) return response('You do not have an active reseller account.');
    return response([
      `**Reseller account**`,
      `Balance: **${formatMoney(account.balance_cents)}**`,
      `Lifetime spend: ${formatMoney(account.total_spent_cents)}`,
      `Keys generated: ${account.total_keys_generated}`
    ].join('\n'));
  }
  if (action !== 'generate') return response('Choose a valid reseller action.');

  const productId = modalComponent(interaction.data, 'reseller_product')?.values?.[0];
  const games = modalComponent(interaction.data, 'reseller_games')?.values || [];
  const quantityText = String(modalComponent(interaction.data, 'reseller_quantity')?.value || '').trim();
  const quantity = /^[1-9]\d{0,3}$/.test(quantityText) ? Number(quantityText) : 0;
  try {
    let order;
    store.transaction((db) => {
      order = generateResellerOrder(db, user.id, productId, quantity, games, randomLicenseKey);
      if (db.service_state?.keys_paused) {
        for (const key of order.keys) pauseLicense(db.keys[key], 'global');
      }
      audit(db, user.id, `reseller-order:${order.orderId}`, 'reseller.keys.generate', 'Reseller self-service order', null, {
        quantity: order.quote.quantity,
        duration_seconds: order.quote.durationSeconds,
        games: [...order.games],
        unit_price_cents: order.quote.unitPriceCents,
        total_cents: order.quote.totalCents,
        balance_cents: order.account.balance_cents
      });
    });

    const keyFile = Buffer.from([
      `Crisis reseller order: ${order.orderId}`,
      `Product: ${order.quote.productLabel}`,
      `Quantity: ${order.quote.quantity}`,
      `Games: ${formatGames(order.games)}`,
      `Tier: ${order.quote.tierLabel} (${order.quote.discountPercent}% discount)`,
      `Unit price: ${formatMoney(order.quote.unitPriceCents)}`,
      `Total: ${formatMoney(order.quote.totalCents)}`,
      '',
      ...order.keys
    ].join('\n'), 'utf8');
    return responseWithFile(
      `Generated **${order.quote.quantity} × ${order.quote.productLabel}** keys for **${formatGames(order.games)}** at ${formatMoney(order.quote.unitPriceCents)} each (${formatMoney(order.quote.totalCents)} total). Remaining balance: **${formatMoney(order.account.balance_cents)}**. Your keys are attached.`,
      `crisis-reseller-${order.orderId.slice(0, 8)}.txt`,
      keyFile
    );
  } catch (error) {
    return response(error.message);
  }
}


async function updateDeferredResponse(interaction, payload, fetchImpl = fetch) {
  const appId = String(interaction.application_id || '').trim();
  const token = String(interaction.token || '').trim();
  if (!/^\d{17,20}$/.test(appId) || !token) throw new Error('Interaction webhook credentials were unavailable');
  const url = `https://discord.com/api/v10/webhooks/${appId}/${token}/messages/@original`;
  let request;
  if (payload.file) {
    const form = new FormData();
    form.set('payload_json', JSON.stringify({ ...payload.data, attachments: [{ id: 0, filename: payload.file.filename }] }));
    form.set('files[0]', new Blob([payload.file.buffer], { type: 'text/plain' }), payload.file.filename);
    request = { method: 'PATCH', body: form };
  } else {
    request = { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload.data) };
  }
  const result = await fetchImpl(url, request);
  if (!result.ok) throw new Error(`Interaction webhook update failed with HTTP ${result.status}`);
}

function verifyInteraction(req) {
  const publicKeyHex = String(process.env.CRISIS_DISCORD_PUBLIC_KEY || '').trim();
  const signatureHex = String(req.get('x-signature-ed25519') || '');
  const timestamp = String(req.get('x-signature-timestamp') || '');
  if (!/^[a-f0-9]{64}$/i.test(publicKeyHex) || !/^[a-f0-9]{128}$/i.test(signatureHex) || !/^\d+$/.test(timestamp)) return false;
  if (Math.abs(Date.now() / 1000 - Number(timestamp)) > 300) return false;
  try {
    const key = crypto.createPublicKey({ key: Buffer.concat([ED25519_SPKI_PREFIX, Buffer.from(publicKeyHex, 'hex')]), format: 'der', type: 'spki' });
    return crypto.verify(null, Buffer.concat([Buffer.from(timestamp), req.body]), key, Buffer.from(signatureHex, 'hex'));
  } catch { return false; }
}

function mountDiscordBot(app, options) {
  app.post('/api/discord/interactions', express.raw({ type: 'application/json', limit: '64kb' }), (req, res) => {
    if (!verifyInteraction(req)) {
      console.warn('[DISCORD] Rejected interaction: signature verification failed.');
      return res.status(401).send('invalid request signature');
    }
    let interaction;
    try { interaction = JSON.parse(req.body.toString('utf8')); }
    catch { return res.status(400).send('invalid JSON'); }
    if (interaction.type === 1) return res.json({ type: 1 });
    if (interaction.type === 5 && [MODULE_MODAL_ID, RESELLER_MODAL_ID].includes(interaction.data?.custom_id)) {
      const reseller = interaction.data.custom_id === RESELLER_MODAL_ID;
      console.log(`[DISCORD] Interaction received: ${reseller ? 'reseller' : 'module'} manager`);
      res.json({ type: 5, data: { flags: EPHEMERAL } });
      const operation = reseller ? handleResellerModal(interaction, options) : handleModuleModal(interaction, options);
      operation
        .then((payload) => updateDeferredResponse(interaction, payload, options.fetchImpl))
        .catch((error) => console.error(`[DISCORD] ${reseller ? 'Reseller' : 'Module'} manager response failed:`, error.message));
      return;
    }
    if (interaction.type !== 2) return res.json(response('Unsupported interaction.'));
    const name = commandName(interaction) || 'unknown';
    const startedAt = Date.now();
    console.log(`[DISCORD] Interaction received: /${name}`);
    try {
      const payload = handleCommand(interaction, options);
      console.log(`[DISCORD] Interaction completed: /${name} (${Date.now() - startedAt}ms)`);
      return res.json(payload);
    }
    catch (error) { console.error('[DISCORD] Command failed:', error.message); return res.json(response('The command could not be completed.')); }
  });
}

async function startDiscordPresence() {
  const token = String(process.env.CRISIS_DISCORD_BOT_TOKEN || '').trim();
  if (!token) {
    console.log('[DISCORD] Gateway presence disabled; configure DISCORD_BOT_TOKEN in .env to enable it.');
    return null;
  }
  if (presenceClient) return presenceClient;

  const { ActivityType, Client } = require('discord.js');
  const client = new Client({
    intents: [],
    presence: {
      status: 'online',
      activities: [{ name: 'license management', type: ActivityType.Watching }]
    }
  });
  client.once('ready', (readyClient) => console.log(`[DISCORD] Gateway online as ${readyClient.user.tag}.`));
  client.on('error', (error) => console.error('[DISCORD] Gateway error:', error.message));
  client.on('warn', (message) => console.warn('[DISCORD] Gateway warning:', message));
  presenceClient = client;
  try {
    await client.login(token);
    return client;
  } catch (error) {
    presenceClient = null;
    client.destroy();
    throw new Error(`Gateway login failed: ${error.message}`);
  }
}

function validateDiscordApplicationConfiguration(application, configuredAppId, configuredPublicKey) {
  if (application?.id !== configuredAppId) {
    throw new Error('DISCORD_CLIENT_ID does not belong to the configured DISCORD_BOT_TOKEN.');
  }
  if (!/^[a-f0-9]{64}$/i.test(configuredPublicKey)) {
    throw new Error('DISCORD_PUBLIC_KEY must be the 64-character Public Key from Discord General Information.');
  }
  if (String(application.verify_key || '').toLowerCase() !== configuredPublicKey.toLowerCase()) {
    throw new Error('DISCORD_PUBLIC_KEY does not belong to the same Discord application as DISCORD_BOT_TOKEN.');
  }
  let endpoint;
  try { endpoint = new URL(application.interactions_endpoint_url); }
  catch { throw new Error('Set the Discord Interactions Endpoint URL to https://wauth.aaddpp.lol/api/discord/interactions.'); }
  if (endpoint.protocol !== 'https:' || endpoint.pathname.replace(/\/$/, '') !== '/api/discord/interactions') {
    throw new Error('Discord has the wrong Interactions Endpoint URL; use https://wauth.aaddpp.lol/api/discord/interactions.');
  }
}

async function registerDiscordCommands() {
  const token = String(process.env.CRISIS_DISCORD_BOT_TOKEN || '').trim();
  const appId = String(process.env.CRISIS_DISCORD_APP_ID || '').trim();
  const publicKey = String(process.env.CRISIS_DISCORD_PUBLIC_KEY || '').trim();
  if (!token || !/^\d{17,20}$/.test(appId)) { console.log('[DISCORD] Bot command registration disabled; configure the server-side bot credentials to enable it.'); return false; }
  const applicationResult = await fetch('https://discord.com/api/v10/oauth2/applications/@me', { headers: { Authorization: `Bot ${token}` } });
  if (!applicationResult.ok) throw new Error(`Discord credential preflight failed with HTTP ${applicationResult.status}`);
  const application = await applicationResult.json();
  validateDiscordApplicationConfiguration(application, appId, publicKey);
  console.log(`[DISCORD] Verified credentials, public key, and interaction endpoint for ${application.name}.`);
  const route = `https://discord.com/api/v10/applications/${appId}/commands`;
  const result = await fetch(route, { method: 'PUT', headers: { Authorization: `Bot ${token}`, 'Content-Type': 'application/json' }, body: JSON.stringify(commandDefinitions()) });
  if (!result.ok) throw new Error(`Discord command registration failed with HTTP ${result.status}`);
  const guildId = String(process.env.CRISIS_DISCORD_GUILD_ID || '').trim();
  if (/^\d{17,20}$/.test(guildId)) {
    const cleanup = await fetch(`https://discord.com/api/v10/applications/${appId}/guilds/${guildId}/commands`, { method: 'PUT', headers: { Authorization: `Bot ${token}`, 'Content-Type': 'application/json' }, body: '[]' });
    if (!cleanup.ok) throw new Error(`Discord guild-command cleanup failed with HTTP ${cleanup.status}`);
  }
  console.log(`[DISCORD] Registered ${commandDefinitions().length} global commands for guild, bot-DM, and user-install contexts.`);
  return true;
}

module.exports = {
  commandDefinitions, downloadDiscordAttachment, handleCommand,
  handleModuleModal, handleResellerModal, issuePairingCode, modulesModal, resellerModal,
  mountDiscordBot, registerDiscordCommands, startDiscordPresence, updateDeferredResponse,
  validateDiscordApplicationConfiguration, verifyInteraction
};
