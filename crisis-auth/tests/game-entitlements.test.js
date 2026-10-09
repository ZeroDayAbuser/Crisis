const assert = require('node:assert/strict');
const test = require('node:test');

const {
  GAME_IDS, formatGames, gameChoices, licenseAllowsGame, parseGameSelection, requiredGameForModule
} = require('../src/lib/game-entitlements');

test('game selections are normalized from one expandable registry', () => {
  assert.deepEqual(GAME_IDS, ['fortnite', 'rust']);
  assert.deepEqual(gameChoices().map((choice) => choice.value), ['fortnite', 'rust', 'all']);
  assert.deepEqual(parseGameSelection('fortnite'), ['fortnite']);
  assert.deepEqual(parseGameSelection(['rust', 'fortnite', 'rust']), ['rust', 'fortnite']);
  assert.deepEqual(parseGameSelection('all'), ['fortnite', 'rust']);
  assert.equal(parseGameSelection(['fortnite', 'unknown']), null);
  assert.equal(formatGames(['fortnite', 'rust']), 'Fortnite + Rust');
});

test('game modules require their entitlement while shared modules remain universal', () => {
  assert.equal(requiredGameForModule('crisis-fortnite.dll'), 'fortnite');
  assert.equal(requiredGameForModule('crisis-fortnite-helper.dll'), 'fortnite');
  assert.equal(requiredGameForModule('fortnite.dll'), 'fortnite');
  assert.equal(requiredGameForModule('crisis-rust.dll'), 'rust');
  assert.equal(requiredGameForModule('crisis-memory.dll'), null);
  assert.equal(requiredGameForModule('rust.dll'), 'rust');
  assert.equal(requiredGameForModule('crisis-overlay.dll'), null);
  assert.equal(requiredGameForModule('crisis-core.dll'), null);
  assert.equal(licenseAllowsGame({ games: ['fortnite'] }, 'fortnite'), true);
  assert.equal(licenseAllowsGame({ games: ['fortnite'] }, 'rust'), false);
});
