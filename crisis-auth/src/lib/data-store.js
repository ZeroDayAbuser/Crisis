const fs = require('fs');
const path = require('path');
const migrations = [
  require('../../migrations/001-initialize-legacy'),
  require('../../migrations/002-accounts-devices-admin'),
  require('../../migrations/003-discord-bot-workflows'),
  require('../../migrations/004-license-pausing'),
  require('../../migrations/005-resellers'),
  require('../../migrations/006-game-entitlements')
];

const CURRENT_SCHEMA_VERSION = 6;

function emptyDatabase() {
  return {
    schema_version: CURRENT_SCHEMA_VERSION,
    keys: {}, players: {}, users: {}, sessions: {}, device_authorizations: {},
    device_credentials: {}, account_license_links: {}, coin_ledger: [], xp_ledger: [],
    subscription_ledger: [], audit_events: [], file_slots: {},
    discord_pairing_codes: {}, hwid_reset_requests: {}, resellers: {}, reseller_ledger: [],
    service_state: { keys_paused: false, keys_paused_at: null }
  };
}

function migrate(database) {
  const db = database && typeof database === 'object' && !Array.isArray(database) ? database : {};
  if (!Number.isSafeInteger(db.schema_version)) db.schema_version = 0;
  for (const migration of migrations) if (db.schema_version < migration.version) { migration.up(db); db.schema_version = migration.version; }
  if (db.schema_version !== CURRENT_SCHEMA_VERSION) throw new Error(`Unsupported database schema ${db.schema_version}`);
  return db;
}

function createStore(filename, options = {}) {
  const databasePath = path.resolve(filename);
  const allowCreate = options.allowCreate !== false;
  let committing = false;

  function read() {
    try { return migrate(JSON.parse(fs.readFileSync(databasePath, 'utf8'))); }
    catch (error) {
      if (error.code === 'ENOENT' && allowCreate) return emptyDatabase();
      throw error;
    }
  }

  function write(database) {
    fs.mkdirSync(path.dirname(databasePath), { recursive: true });
    const payload = `${JSON.stringify(migrate(database), null, 2)}\n`;
    const temporary = `${databasePath}.${process.pid}.${Date.now()}.tmp`;
    try {
      try {
        if (fs.readFileSync(databasePath, 'utf8') === payload) return true;
      } catch (error) {
        if (error.code !== 'ENOENT') throw error;
      }
      fs.writeFileSync(temporary, payload, { encoding: 'utf8', mode: 0o600, flag: 'wx' });
      fs.renameSync(temporary, databasePath);
      return true;
    } catch (error) {
      try { fs.unlinkSync(temporary); } catch {}
      console.error('[DB] Atomic commit failed:', error.message);
      return false;
    }
  }

  function transaction(callback) {
    if (committing) throw new Error('Nested database transactions are not supported');
    committing = true;
    try {
      const database = read();
      const result = callback(database);
      if (result && typeof result.then === 'function') throw new Error('Database transaction callbacks must be synchronous');
      if (!write(database)) throw new Error('Database commit failed');
      return result;
    } finally { committing = false; }
  }

  function initialize() {
    const database = read();
    if (!fs.existsSync(databasePath) || database.schema_version !== JSON.parse(fs.readFileSync(databasePath, 'utf8')).schema_version) write(database);
    return database;
  }

  return { databasePath, initialize, read, transaction, write };
}

module.exports = { CURRENT_SCHEMA_VERSION, createStore, emptyDatabase, migrate };
