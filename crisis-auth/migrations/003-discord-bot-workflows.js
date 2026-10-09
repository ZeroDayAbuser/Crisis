module.exports = {
  version: 3,
  description: 'Add one-time Discord pairing codes and audited HWID reset requests.',
  up(db) {
    for (const field of ['discord_pairing_codes', 'hwid_reset_requests']) {
      if (!db[field] || typeof db[field] !== 'object' || Array.isArray(db[field])) db[field] = {};
    }
  }
};
