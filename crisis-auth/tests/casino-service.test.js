const assert = require('assert/strict');
const crypto = require('crypto');
const http = require('http');
const test = require('node:test');
const express = require('express');
const { authenticate, bindPlayerToLicense, mountPlayerRoutes } = require('../src/player-service');
const { commitment, fairInt, minesMultiplier } = require('../src/casino-service');

const KEY = 'CASINO-TEST-KEY';
const HWID = 'CASINO-TEST-HWID';
const DISCORD_ID = '333333333333333333';
const CLIENT_SEED = 'client-seed-for-tests';
const ACCESS_TOKEN = 'casino-test-access-token';

function request(port, method, path, body) {
  return new Promise((resolve, reject) => {
    const payload = body === undefined ? '' : JSON.stringify(body);
    const req = http.request({ hostname: '127.0.0.1', port, method, path, headers: {
      'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(payload),
      Authorization: `Bearer ${ACCESS_TOKEN}`
    } }, (res) => {
      const chunks = [];
      res.on('data', (chunk) => chunks.push(chunk));
      res.on('end', () => resolve({ status: res.statusCode, body: JSON.parse(Buffer.concat(chunks).toString('utf8')) }));
    });
    req.on('error', reject); req.end(payload);
  });
}

function id(label) { return `casino-test-${label}-request`; }

test('fair integer mapping is deterministic, bounded, and rejection-unbiased', () => {
  const seed = '11'.repeat(32);
  const values = Array.from({ length: 100 }, (_, cursor) => fairInt(seed, CLIENT_SEED, 7, 'roulette', 37, cursor));
  assert.ok(values.every((value) => Number.isInteger(value) && value >= 0 && value < 37));
  assert.deepEqual(values, Array.from({ length: 100 }, (_, cursor) => fairInt(seed, CLIENT_SEED, 7, 'roulette', 37, cursor)));
  assert.equal(commitment(seed), crypto.createHash('sha256').update(seed).digest('hex'));
});

test('mines payout uses the published 1% house factor', () => {
  assert.equal(minesMultiplier(3, 0), 0.99);
  assert.equal(minesMultiplier(3, 1), 1.12);
  assert.ok(minesMultiplier(8, 4) > minesMultiplier(3, 4));
});

test('all casino balances and outcomes are server authoritative and provably fair', async () => {
  const now = Math.floor(Date.now() / 1000);
  const db = { keys: { [KEY]: { hwid: HWID, start_unix: now - 10, expiry_unix: now + 3600 } }, players: {}, coin_ledger: [] };
  const player = bindPlayerToLicense(db, KEY, DISCORD_ID, { username: 'casino' }).player;
  player.crisis_coins = 50_000;
  const app = express();
  mountPlayerRoutes(app, {
    loadDatabase: () => db,
    saveDatabase: () => true,
    authenticateAccess: (req, currentDb) =>
      req.get('authorization') === `Bearer ${ACCESS_TOKEN}` ? authenticate(currentDb, KEY, HWID) : null,
    casinoRateLimit: { windowMs: 60_000, max: 100 }
  });
  const server = await new Promise((resolve) => { const value = app.listen(0, '127.0.0.1', () => resolve(value)); });
  const port = server.address().port;
  try {
    let result = await request(port, 'GET', '/v1/player/casino/state');
    assert.equal(result.status, 200);
    const committedBeforeRoulette = result.body.fairness.commitment;
    const rouletteRequest = { bet: 100, choice: 'red', client_seed: CLIENT_SEED, request_id: id('roulette') };
    result = await request(port, 'POST', '/v1/player/casino/roulette', rouletteRequest);
    assert.equal(result.status, 200);
    assert.equal(commitment(result.body.game.proof.server_seed), committedBeforeRoulette);
    assert.equal(result.body.game.number, fairInt(result.body.game.proof.server_seed, CLIENT_SEED, result.body.game.proof.nonce, 'roulette', 37));
    const rouletteBalance = result.body.balance;
    const replayed = await request(port, 'POST', '/v1/player/casino/roulette', rouletteRequest);
    assert.deepEqual(replayed.body, result.body, 'an idempotent retry cannot charge or pay twice');
    assert.equal(db.players[DISCORD_ID].crisis_coins, rouletteBalance);

    result = await request(port, 'POST', '/v1/player/casino/mines/start', { bet: 250, mines: 5, client_seed: CLIENT_SEED, request_id: id('mines-start') });
    assert.equal(result.status, 200);
    assert.equal(result.body.game.proof.server_seed, null, 'the seed stays secret during an active round');
    assert.equal(result.body.game.mine_positions, null, 'unrevealed mines never leave the server');
    const hiddenRound = db.players[DISCORD_ID].casino.active.mines;
    const activeBalance = db.players[DISCORD_ID].crisis_coins;
    const concurrentRound = await request(port, 'POST', '/v1/player/casino/chicken/start', {
      bet: 100, risk: 'safe', client_seed: CLIENT_SEED, request_id: id('chicken-concurrent')
    });
    assert.equal(concurrentRound.status, 409, 'only one interactive casino round may be active');
    assert.equal(db.players[DISCORD_ID].crisis_coins, activeBalance, 'a rejected concurrent round cannot debit coins');
    const safeTile = Array.from({ length: 25 }, (_, index) => index).find((tile) => !hiddenRound.mine_positions.includes(tile));
    result = await request(port, 'POST', '/v1/player/casino/mines/reveal', { tile: safeTile, request_id: id('mines-reveal') });
    assert.equal(result.body.game.status, 'playing');
    assert.deepEqual(result.body.game.revealed, [safeTile]);
    result = await request(port, 'POST', '/v1/player/casino/mines/cashout', { request_id: id('mines-cashout') });
    assert.equal(result.body.game.status, 'cashed_out');
    assert.equal(commitment(result.body.game.proof.server_seed), result.body.game.proof.commitment);
    assert.equal(result.body.game.payout, Math.floor(250 * minesMultiplier(5, 1)));

    result = await request(port, 'POST', '/v1/player/casino/chicken/start', { bet: 100, risk: 'safe', client_seed: CLIENT_SEED, request_id: id('chicken-start') });
    assert.equal(result.status, 200);
    const chicken = db.players[DISCORD_ID].casino.active.chicken;
    const survives = fairInt(chicken.server_seed, chicken.client_seed, chicken.nonce, 'chicken-0', 10000) < 8500;
    result = await request(port, 'POST', '/v1/player/casino/chicken/jump', { request_id: id('chicken-jump') });
    assert.equal(result.body.game.caught, !survives);
    if (survives) {
      result = await request(port, 'POST', '/v1/player/casino/chicken/cashout', { request_id: id('chicken-cashout') });
      assert.equal(result.body.game.status, 'cashed_out');
    }

    result = await request(port, 'POST', '/v1/player/casino/crash', { bet: 100, target: 2, client_seed: CLIENT_SEED, request_id: id('crash') });
    assert.equal(result.status, 200);
    assert.ok(result.body.game.crash_point >= 1);
    assert.equal(result.body.game.won, result.body.game.crash_point >= 2);
    assert.equal(commitment(result.body.game.proof.server_seed), result.body.game.proof.commitment);

    result = await request(port, 'POST', '/v1/player/casino/crate', { bet: 15, client_seed: CLIENT_SEED, request_id: id('crate') });
    assert.equal(result.status, 200);
    assert.equal(result.body.game.kind, 'crate');
    assert.ok(result.body.game.feature_pool.includes(result.body.game.feature));
    assert.ok(db.players[DISCORD_ID].unlocked_features.includes(result.body.game.feature));
    assert.equal(commitment(result.body.game.proof.server_seed), result.body.game.proof.commitment);

    result = await request(port, 'POST', '/v1/player/casino/blackjack/start', { bet: 100, client_seed: CLIENT_SEED, request_id: id('blackjack-start') });
    assert.equal(result.status, 200);
    assert.equal(result.body.game.dealer_cards[1] === '??' || result.body.game.proof.server_seed !== null, true);
    if (result.body.game.status === 'playing') {
      result = await request(port, 'POST', '/v1/player/casino/blackjack/action', { action: 'stand', request_id: id('blackjack-stand') });
      assert.notEqual(result.body.game.status, 'playing');
    }
    assert.equal(commitment(result.body.game.proof.server_seed), result.body.game.proof.commitment);

    assert.ok(db.coin_ledger.some((entry) => entry.actor_role === 'casino' && entry.reason.includes('wager')));
    assert.ok(db.coin_ledger.every((entry) => Number.isSafeInteger(entry.balance) && entry.balance >= 0));
    assert.equal((await request(port, 'POST', '/v1/player/casino/roulette', { ...rouletteRequest, bet: 9_999_999, request_id: id('oversized') })).status, 400);
  } finally {
    await new Promise((resolve, reject) => server.close((error) => error ? reject(error) : resolve()));
  }
});
