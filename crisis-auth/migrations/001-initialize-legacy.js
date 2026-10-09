module.exports = {
  version: 1,
  description: 'Normalize the legacy keys and players collections.',
  up(db) {
    for (const field of ['keys', 'players']) if (!db[field] || typeof db[field] !== 'object' || Array.isArray(db[field])) db[field] = {};
  }
};
