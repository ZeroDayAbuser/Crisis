module.exports = {
  version: 2,
  description: 'Add accounts, sessions, device authorization, ledgers, audit events, and file slots.',
  up(db) {
    for (const field of ['users', 'sessions', 'device_authorizations', 'device_credentials', 'account_license_links', 'file_slots']) if (!db[field] || typeof db[field] !== 'object' || Array.isArray(db[field])) db[field] = {};
    for (const field of ['coin_ledger', 'subscription_ledger', 'audit_events']) if (!Array.isArray(db[field])) db[field] = [];
  }
};
