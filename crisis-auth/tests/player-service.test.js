const assert = require('assert/strict');
const http = require('http');
const express = require('express');
const { authenticate, bindPlayerToLicense, ensurePlayer, mountPlayerRoutes, scoreboard } = require('../src/player-service');

const KEY = 'PLAYER-TEST-KEY';
const HWID = 'PLAYER-TEST-HWID';
const DISCORD_ID = '111111111111111111';
const EVENT_SECRET = 'test-event-secret-that-is-at-least-32-bytes';
const ACCESS_TOKEN = 'player-test-access-token';

function request(port, method, path, body, extraHeaders = {}) {
  return new Promise((resolve, reject) => {
    const payload = body === undefined ? '' : JSON.stringify(body);
    const req = http.request({ hostname: '127.0.0.1', port, method, path, headers: {
      'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(payload),
      Authorization: `Bearer ${ACCESS_TOKEN}`, ...extraHeaders
    } }, (res) => {
      const chunks = [];
      res.on('data', (chunk) => chunks.push(chunk));
      res.on('end', () => resolve({ status: res.statusCode, body: JSON.parse(Buffer.concat(chunks).toString('utf8')) }));
    });
    req.on('error', reject);
    req.end(payload);
  });
}

async function main() {
  const now = Math.floor(Date.now() / 1000);
  const db = { keys: { [KEY]: { hwid: HWID, start_unix: now - 10, expiry_unix: now + 3600 } }, players: {} };
  const bound = bindPlayerToLicense(db, KEY, DISCORD_ID, { username: 'tester', display_name: 'Test Player' });
  assert.ok(bound.player);
  assert.equal(db.keys[KEY].discord_id, DISCORD_ID);
  assert.equal(bindPlayerToLicense(db, KEY, '222222222222222222').error, 'License is already linked to another Discord profile');

  for (let i = 0; i < 8; ++i) {
    const id = String(222222222222222222n + BigInt(i));
    const player = ensurePlayer(db, id, { display_name: `Nearby ${i}` });
    player.lifetime_xp = (i + 1) * 1000;
  }

  let randomValue = 0;
  const app = express();
  mountPlayerRoutes(app, {
    loadDatabase: () => db,
    saveDatabase: () => true,
    random: () => randomValue,
    eventSecret: EVENT_SECRET,
    authenticateAccess: (req, currentDb) =>
      req.get('authorization') === `Bearer ${ACCESS_TOKEN}` ? authenticate(currentDb, KEY, HWID) : null,
    casinoRateLimit: { windowMs: 60000, max: 8 }
  });
  const server = await new Promise((resolve) => { const value = app.listen(0, '127.0.0.1', () => resolve(value)); });
  const port = server.address().port;
  try {
    let result = await request(port, 'GET', '/v1/player');
    assert.equal(result.status, 200);
    assert.equal(result.body.player.discord_id, DISCORD_ID);
    assert.equal(result.body.player.role, 'Member');
    assert.equal(result.body.player.level.level, 1);
    assert.equal(result.body.subscription.expires_unix, db.keys[KEY].expiry_unix);
    assert.equal(result.body.scoreboard.entries.length, 7);
    assert.ok(result.body.scoreboard.entries.some((entry) => entry.current_user));

    result = await request(port, 'PATCH', '/v1/player/stats', { stats: { winrate: 100, level: 999, kd: 999 } });
    assert.equal(result.status, 410);
    assert.match(result.body.error, /server-authoritative/i);

    result = await request(port, 'POST', '/v1/player/gameplay/match/start', {
      match_id: 'match-profile-test-0001', request_id: 'request-profile-test-0001', mode: 'ranked'
    });
    assert.equal(result.status, 200);
    result = await request(port, 'POST', '/v1/player/gameplay/elimination', {
      match_id: 'match-profile-test-0001', event_id: 'event-elimination-0001'
    });
    assert.equal(result.status, 200);
    assert.equal(result.body.earning.amount, 50);
    const duplicate = await request(port, 'POST', '/v1/player/gameplay/elimination', {
      match_id: 'match-profile-test-0001', event_id: 'event-elimination-0001'
    });
    assert.equal(duplicate.status, 200);
    assert.equal(duplicate.body.duplicate, true);
    assert.equal(duplicate.body.player.lifetime_xp, 50);
    db.players[DISCORD_ID].gameplay.active_match.started_at -= 61;
    result = await request(port, 'POST', '/v1/player/gameplay/match/end', {
      match_id: 'match-profile-test-0001', event_id: 'event-match-end-0001', won: true
    });
    assert.equal(result.status, 200);
    assert.equal(result.body.earning.amount, 150);
    assert.equal(result.body.player.xp_balance, 200);
    assert.equal(result.body.player.lifetime_xp, 200);
    assert.deepEqual({
      games: result.body.player.stats.games_played, wins: result.body.player.stats.wins,
      kills: result.body.player.stats.kills, ranked_games: result.body.player.stats.ranked_games,
      ranked_kills: result.body.player.stats.ranked_kills
    }, { games: 1, wins: 1, kills: 1, ranked_games: 1, ranked_kills: 1 });
    assert.equal(result.body.player.stats.winrate, 100);
    assert.equal(result.body.player.level.level, 1);

    assert.equal((await request(port, 'POST', '/v1/player/xp/earn', { amount: 1000, event: 'Forged' })).status, 403);
    assert.equal((await request(port, 'POST', '/v1/player/xp/earn', { amount: 1000, event: 'Forged' },
      { 'X-Crisis-Event-Secret': 'incorrect-secret-that-is-at-least-32-bytes' })).status, 403);
    for (let i = 1; i <= 9; ++i) {
      result = await request(port, 'POST', '/v1/player/xp/earn', { amount: 1000, event: `Event ${i}` },
        { 'X-Crisis-Event-Secret': EVENT_SECRET });
      assert.equal(result.status, 200);
    }
    assert.equal(result.body.player.xp_balance, 9200);
    assert.equal(result.body.player.lifetime_xp, 9200);
    assert.equal(result.body.player.recent_xp_earnings.length, 7);
    assert.equal(result.body.player.recent_xp_earnings[0].event, 'Event 3');
    assert.equal(result.body.player.lifetime_xp_progress.length, 11);
    assert.equal(result.body.player.lifetime_xp_progress[10].lifetime_xp, 9200);
    assert.equal(result.body.player.level.level, 5);

    result = await request(port, 'POST', '/v1/player/coins/purchase', { coins: 1250 });
    assert.equal(result.status, 200);
    assert.equal(result.body.xp_spent, 6250);
    assert.equal(result.body.player.xp_balance, 2950);
    assert.equal(result.body.player.lifetime_xp, 9200);
    assert.equal(result.body.player.crisis_coins, 1250);
    assert.equal((await request(port, 'POST', '/v1/player/coins/purchase', { coins: 1000 })).status, 409);

    db.players[DISCORD_ID].xp_balance = 8000;
    const xpExpiryBefore = db.keys[KEY].expiry_unix;
    result = await request(port, 'POST', '/v1/player/subscription/redeem-xp', { hours: 3 });
    assert.equal(result.status, 200);
    assert.equal(result.body.xp_spent, 8000);
    assert.equal(result.body.player.xp_balance, 0);
    assert.equal(result.body.expiry_unix, xpExpiryBefore + 3 * 60 * 60);
    assert.equal((await request(port, 'POST', '/v1/player/subscription/redeem-xp', { hours: 4 })).status, 400);

    for (const legacyPath of ['/v1/player/gamble', '/v1/player/games/roulette', '/v1/player/games/blackjack/start', '/v1/player/games/blackjack/action', '/v1/player/crates/open']) {
      const retired = await request(port, 'POST', legacyPath, {});
      assert.equal(retired.status, 410);
      assert.match(retired.body.error, /legacy casino route retired/i);
    }

    assert.equal((await request(port, 'POST', '/v1/player/subscription/redeem', { days: 2 })).status, 400);
    db.players[DISCORD_ID].crisis_coins = 100000;
    const expiryBefore = db.keys[KEY].expiry_unix;
    result = await request(port, 'POST', '/v1/player/subscription/redeem', { days: 1 });
    assert.equal(result.status, 200);
    assert.equal(result.body.seconds_added, 86400);
    assert.equal(result.body.coins_spent, 2000);
    assert.equal(result.body.expiry_unix, expiryBefore + 86400);
    result = await request(port, 'POST', '/v1/player/subscription/redeem', { days: 30 });
    assert.equal(result.status, 200);
    assert.equal(result.body.seconds_added, 30 * 86400);
    assert.equal(result.body.coins_spent, 48000);
    assert.equal(result.body.expiry_unix, expiryBefore + 31 * 86400);
    result = await request(port, 'POST', '/v1/player/subscription/redeem', { days: 1 });
    assert.equal(result.status, 200);
    assert.equal(result.body.coins_spent, 2000);
    assert.equal(result.body.expiry_unix, expiryBefore + 32 * 86400);

    const board = scoreboard(db, DISCORD_ID);
    assert.equal(board.placement, 1);
    assert.equal(board.entries[0].current_user, true);

    const unauthorized = await new Promise((resolve, reject) => {
      http.get(`http://127.0.0.1:${port}/v1/player`, (res) => { res.resume(); res.on('end', () => resolve(res.statusCode)); }).on('error', reject);
    });
    assert.equal(unauthorized, 401);
    console.log('Player service tests passed.');
  } finally {
    await new Promise((resolve, reject) => server.close((error) => error ? reject(error) : resolve()));
  }
}

main().catch((error) => { console.error(error); process.exit(1); });
