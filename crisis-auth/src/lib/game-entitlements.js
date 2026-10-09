const GAMES = Object.freeze({
  fortnite: Object.freeze({ label: 'Fortnite', modulePrefix: 'crisis-fortnite' }),
  rust: Object.freeze({ label: 'Rust', modulePrefix: 'crisis-rust' })
});

const GAME_IDS = Object.freeze(Object.keys(GAMES));

function parseGameSelection(value) {
  if (value === 'all') return [...GAME_IDS];
  const values = Array.isArray(value) ? value : [value];
  if (!values.length || values.some((game) => typeof game !== 'string' || !GAMES[game])) return null;
  return [...new Set(values)];
}

function gameChoices(includeAll = true) {
  const choices = GAME_IDS.map((value) => ({ name: GAMES[value].label, value }));
  if (includeAll && GAME_IDS.length > 1) choices.push({ name: 'All Games', value: 'all' });
  return choices;
}

function formatGames(games) {
  const normalized = parseGameSelection(games);
  return normalized ? normalized.map((game) => GAMES[game].label).join(' + ') : 'No games';
}

function requiredGameForModule(filename) {
  const normalized = String(filename || '').trim().toLowerCase();
  for (const [game, definition] of Object.entries(GAMES)) {
    if (normalized === `${definition.modulePrefix}.dll` || normalized.startsWith(`${definition.modulePrefix}-`) ||
        normalized === `${game}.dll` || normalized.startsWith(`${game}-`)) return game;
  }
  return null;
}

function licenseAllowsGame(license, game) {
  if (!GAMES[game]) return false;
  const games = parseGameSelection(license?.games);
  return Boolean(games?.includes(game));
}

module.exports = {
  GAMES, GAME_IDS, formatGames, gameChoices, licenseAllowsGame, parseGameSelection, requiredGameForModule
};
