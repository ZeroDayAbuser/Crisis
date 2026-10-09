const LEGACY_GAMES = Object.freeze(['fortnite', 'rust']);

module.exports = {
  version: 6,
  description: 'Add explicit game entitlements to license keys.',
  up(db) {
    for (const license of Object.values(db.keys || {})) {
      const games = Array.isArray(license.games)
        ? license.games.filter((game) => LEGACY_GAMES.includes(game))
        : LEGACY_GAMES;
      license.games = [...new Set(games.length ? games : LEGACY_GAMES)];
    }
  }
};
