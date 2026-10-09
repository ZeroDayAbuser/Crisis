const crypto = require('crypto');

const MAX_BET = 1_000_000;
const CLIENT_SEED = /^[A-Za-z0-9._:-]{8,96}$/;
const REQUEST_ID = /^[A-Za-z0-9_-]{12,96}$/;
const ROULETTE_RED = new Set([1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36]);
const ROULETTE_CHOICES = new Set(['red', 'black', 'green']);
const MINES_COUNTS = new Set([3, 5, 8]);
const CHICKEN_RULES = Object.freeze({
  safe: Object.freeze({ survival: 8500, multiplier: 1.15 }),
  spicy: Object.freeze({ survival: 6500, multiplier: 1.50 }),
  wild: Object.freeze({ survival: 3500, multiplier: 2.75 })
});
const CRASH_TARGETS = new Set([1.5, 2, 5, 10]);
const CRATE_FEATURES = Object.freeze([
  'spin_bot', 'rapid_fire', 'wall_pierce', 'silent_aim', 'fov_changer', 'car_fly',
  'infinite_fuel', 'chams', 'no_spread', 'no_recoil', 'magic_bullet', 'fire_rate'
]);

function nowUnix() { return Math.floor(Date.now() / 1000); }
function newSeed() { return crypto.randomBytes(32).toString('hex'); }
function commitment(seed) { return crypto.createHash('sha256').update(seed, 'utf8').digest('hex'); }

function ensureCasino(player) {
  if (!player.casino || typeof player.casino !== 'object' || Array.isArray(player.casino)) player.casino = {};
  const casino = player.casino;
  if (typeof casino.server_seed !== 'string' || !/^[a-f0-9]{64}$/.test(casino.server_seed)) casino.server_seed = newSeed();
  casino.commitment = commitment(casino.server_seed);
  if (!Number.isSafeInteger(casino.nonce) || casino.nonce < 0) casino.nonce = 0;
  if (!casino.active || typeof casino.active !== 'object' || Array.isArray(casino.active)) casino.active = {};
  if (!casino.requests || typeof casino.requests !== 'object' || Array.isArray(casino.requests)) casino.requests = {};
  if (!Array.isArray(casino.request_order)) casino.request_order = [];
  if (!Array.isArray(casino.history)) casino.history = [];
  return casino;
}

function publicFairness(player) {
  const casino = ensureCasino(player);
  return {
    algorithm: 'HMAC-SHA256/v1',
    commitment: casino.commitment,
    nonce: casino.nonce,
    rules: {
      roulette: { pockets: 37, payouts: { red: 2, black: 2, green: 36 } },
      mines: { board_size: 25, mine_counts: [3, 5, 8], house_factor: 0.99 },
      blackjack: { decks: 1, dealer_stands_on: 17, blackjack_payout: 1.5 },
      chicken: CHICKEN_RULES,
      crash: { targets: [1.5, 2, 5, 10], house_factor: 0.99 }
    }
  };
}

function digest(seed, clientSeed, nonce, game, cursor = 0) {
  return crypto.createHmac('sha256', Buffer.from(seed, 'hex'))
    .update(`${clientSeed}:${nonce}:${game}:${cursor}`, 'utf8').digest();
}

function fairInt(seed, clientSeed, nonce, game, max, cursor = 0) {
  if (!Number.isSafeInteger(max) || max < 1 || max > 0x100000000) throw new RangeError('invalid fair range');
  const limit = Math.floor(0x100000000 / max) * max;
  for (let attempt = 0; attempt < 1024; attempt += 1) {
    const value = digest(seed, clientSeed, nonce, game, cursor + attempt).readUInt32BE(0);
    if (value < limit) return value % max;
  }
  throw new Error('Unable to draw an unbiased result');
}

function shuffledDeck(seed, clientSeed, nonce) {
  const suits = ['S', 'H', 'D', 'C'];
  const ranks = ['A', '2', '3', '4', '5', '6', '7', '8', '9', '10', 'J', 'Q', 'K'];
  const deck = [];
  for (const suit of suits) for (const rank of ranks) deck.push(`${rank}${suit}`);
  let cursor = 0;
  for (let index = deck.length - 1; index > 0; index -= 1) {
    const swap = fairInt(seed, clientSeed, nonce, 'blackjack', index + 1, cursor);
    cursor += 1;
    [deck[index], deck[swap]] = [deck[swap], deck[index]];
  }
  return deck;
}

function handValue(cards) {
  let total = 0;
  let aces = 0;
  for (const card of cards || []) {
    const rank = String(card).slice(0, -1);
    if (rank === 'A') { total += 11; aces += 1; }
    else if (['J', 'Q', 'K'].includes(rank)) total += 10;
    else total += Number(rank) || 0;
  }
  while (total > 21 && aces > 0) { total -= 10; aces -= 1; }
  return total;
}

function validateCommon(body) {
  const bet = body?.bet;
  const clientSeed = body?.client_seed;
  const requestId = body?.request_id;
  if (!Number.isSafeInteger(bet) || bet < 1 || bet > MAX_BET) return { error: `bet must be an integer from 1 to ${MAX_BET}` };
  if (typeof clientSeed !== 'string' || !CLIENT_SEED.test(clientSeed)) return { error: 'client_seed must be 8-96 safe characters' };
  if (typeof requestId !== 'string' || !REQUEST_ID.test(requestId)) return { error: 'request_id must be 12-96 safe characters' };
  return { bet, clientSeed, requestId };
}

function requireRequestId(body) {
  const requestId = body?.request_id;
  return typeof requestId === 'string' && REQUEST_ID.test(requestId) ? requestId : null;
}

function beginRound(player, game, bet, clientSeed) {
  const casino = ensureCasino(player);
  const activeGame = Object.keys(casino.active)[0];
  if (activeGame) throw conflict(`Finish the active ${activeGame} round first`);
  if (player.crisis_coins < bet) throw conflict('Insufficient Crisis Coins');
  const round = {
    id: crypto.randomUUID(), game, bet, client_seed: clientSeed,
    server_seed: casino.server_seed, commitment: casino.commitment,
    nonce: casino.nonce, started_at: nowUnix(), status: 'playing'
  };
  casino.nonce += 1;
  casino.server_seed = newSeed();
  casino.commitment = commitment(casino.server_seed);
  casino.active[game] = round;
  player.crisis_coins -= bet;
  return round;
}

function proof(player, round, reveal) {
  const casino = ensureCasino(player);
  return {
    algorithm: 'HMAC-SHA256/v1', commitment: round.commitment,
    server_seed: reveal ? round.server_seed : null,
    client_seed: round.client_seed, nonce: round.nonce,
    next_commitment: casino.commitment
  };
}

function recordCoin(db, player, delta, previous, reason, round) {
  if (!Array.isArray(db.coin_ledger)) db.coin_ledger = [];
  db.coin_ledger.push({
    id: crypto.randomUUID(), discord_id: player.discord_id, delta, previous,
    balance: player.crisis_coins, reason, actor_role: 'casino',
    round_id: round.id, game: round.game, created_at: nowUnix()
  });
}

function recordWager(db, player, round) {
  recordCoin(db, player, -round.bet, player.crisis_coins + round.bet, `${round.game} wager`, round);
}

function publicRound(player, round, reveal = false) {
  const base = {
    id: round.id, game: round.game, kind: round.game, bet: round.bet, status: round.status,
    payout: round.payout || 0, coin_change: (round.payout || 0) - round.bet,
    balance: player.crisis_coins, started_at: round.started_at,
    settled_at: round.settled_at || null, proof: proof(player, round, reveal)
  };
  if (round.game === 'mines') Object.assign(base, {
    mines: round.mines, revealed: [...round.revealed], multiplier: round.multiplier,
    mine_positions: reveal ? [...round.mine_positions] : null
  });
  if (round.game === 'roulette') Object.assign(base, { number: round.number, color: round.color, choice: round.choice, won: round.won });
  if (round.game === 'blackjack') Object.assign(base, {
    player_cards: [...round.player_cards],
    dealer_cards: reveal ? [...round.dealer_cards] : [round.dealer_cards[0], '??'],
    player_total: handValue(round.player_cards), dealer_total: reveal ? handValue(round.dealer_cards) : null,
    message: round.message
  });
  if (round.game === 'chicken') Object.assign(base, { risk: round.risk, crossed: round.crossed, multiplier: round.multiplier, caught: round.caught });
  if (round.game === 'crash') Object.assign(base, { target: round.target, crash_point: round.crash_point, won: round.won });
  if (round.game === 'crate') Object.assign(base, { feature: round.feature, feature_pool: round.feature_pool });
  return base;
}

function settle(db, player, round, status, payout, message) {
  const casino = ensureCasino(player);
  const previous = player.crisis_coins;
  round.status = status;
  round.payout = payout;
  round.message = message;
  round.settled_at = nowUnix();
  player.crisis_coins += payout;
  if (payout) recordCoin(db, player, payout, previous, `${round.game} payout (${status})`, round);
  delete casino.active[round.game];
  const history = publicRound(player, round, true);
  casino.history.push(history);
  casino.history = casino.history.slice(-50);
  return history;
}

function conflict(message) { const error = new Error(message); error.status = 409; return error; }

function remember(player, requestId, payload) {
  const casino = ensureCasino(player);
  casino.requests[requestId] = payload;
  casino.request_order.push(requestId);
  while (casino.request_order.length > 100) delete casino.requests[casino.request_order.shift()];
  return payload;
}

function replay(player, requestId) {
  const casino = ensureCasino(player);
  return Object.prototype.hasOwnProperty.call(casino.requests, requestId) ? casino.requests[requestId] : null;
}

function rouletteColor(number) { return number === 0 ? 'green' : ROULETTE_RED.has(number) ? 'red' : 'black'; }

function minesMultiplier(mines, reveals) {
  let multiplier = 0.99;
  for (let step = 0; step < reveals; step += 1) multiplier *= (25 - step) / (25 - mines - step);
  return Math.floor(multiplier * 100) / 100;
}

function createCasinoRoutes(router, { limitCasinoActions, sendMutation }) {
  router.get('/casino/state', (req, res) => {
    return sendMutation(req, res, ({ player }) => {
      const casino = ensureCasino(player);
      const active = {};
      for (const [game, round] of Object.entries(casino.active)) active[game] = publicRound(player, round, false);
      return { success: true, balance: player.crisis_coins, fairness: publicFairness(player), active, history: casino.history.slice(-10) };
    });
  });

  router.post('/casino/roulette', limitCasinoActions, (req, res) => {
    const common = validateCommon(req.body);
    const choice = typeof req.body?.choice === 'string' ? req.body.choice.toLowerCase() : '';
    if (common.error) return res.status(400).json({ success: false, error: common.error });
    if (!ROULETTE_CHOICES.has(choice)) return res.status(400).json({ success: false, error: 'choice must be red, black, or green' });
    return sendMutation(req, res, ({ db, player }) => {
      const old = replay(player, common.requestId); if (old) return old;
      const round = beginRound(player, 'roulette', common.bet, common.clientSeed);
      recordWager(db, player, round);
      round.choice = choice;
      round.number = fairInt(round.server_seed, round.client_seed, round.nonce, 'roulette', 37);
      round.color = rouletteColor(round.number);
      round.won = round.color === choice;
      const payout = round.won ? round.bet * (choice === 'green' ? 36 : 2) : 0;
      const game = settle(db, player, round, round.won ? 'won' : 'lost', payout, round.won ? 'Winning pocket' : 'No match');
      player.updated_at = nowUnix();
      return remember(player, common.requestId, { success: true, balance: player.crisis_coins, game });
    });
  });

  router.post('/casino/crash', limitCasinoActions, (req, res) => {
    const common = validateCommon(req.body);
    const target = Number(req.body?.target);
    if (common.error) return res.status(400).json({ success: false, error: common.error });
    if (!CRASH_TARGETS.has(target)) return res.status(400).json({ success: false, error: 'target must be 1.5, 2, 5, or 10' });
    return sendMutation(req, res, ({ db, player }) => {
      const old = replay(player, common.requestId); if (old) return old;
      const round = beginRound(player, 'crash', common.bet, common.clientSeed);
      recordWager(db, player, round);
      const roll = fairInt(round.server_seed, round.client_seed, round.nonce, 'crash', 100_000_000) / 100_000_000;
      round.target = target;
      round.crash_point = Math.min(1000, Math.max(1, Math.floor((0.99 / Math.max(Number.EPSILON, 1 - roll)) * 100) / 100));
      round.won = round.crash_point >= target;
      const game = settle(db, player, round, round.won ? 'won' : 'lost', round.won ? Math.floor(round.bet * target) : 0, round.won ? 'Auto cash-out reached' : 'Crashed before target');
      player.updated_at = nowUnix();
      return remember(player, common.requestId, { success: true, balance: player.crisis_coins, game });
    });
  });

  router.post('/casino/crate', limitCasinoActions, (req, res) => {
    const common = validateCommon(req.body);
    if (common.error) return res.status(400).json({ success: false, error: common.error });
    return sendMutation(req, res, ({ db, player }) => {
      const old = replay(player, common.requestId); if (old) return old;
      const locked = CRATE_FEATURES.filter((feature) => !player.unlocked_features.includes(feature));
      if (!locked.length) throw conflict('Every crate feature is already unlocked');
      const round = beginRound(player, 'crate', common.bet, common.clientSeed);
      recordWager(db, player, round);
      round.feature_pool = locked;
      round.feature = locked[fairInt(round.server_seed, round.client_seed, round.nonce, 'crate', locked.length)];
      player.unlocked_features.push(round.feature);
      const game = settle(db, player, round, 'won', 0, `Unlocked ${round.feature}`);
      player.updated_at = nowUnix();
      return remember(player, common.requestId, { success: true, balance: player.crisis_coins, game });
    });
  });

  router.post('/casino/mines/start', limitCasinoActions, (req, res) => {
    const common = validateCommon(req.body);
    const mines = req.body?.mines;
    if (common.error) return res.status(400).json({ success: false, error: common.error });
    if (!MINES_COUNTS.has(mines)) return res.status(400).json({ success: false, error: 'mines must be 3, 5, or 8' });
    return sendMutation(req, res, ({ db, player }) => {
      const old = replay(player, common.requestId); if (old) return old;
      const round = beginRound(player, 'mines', common.bet, common.clientSeed);
      recordWager(db, player, round);
      round.mines = mines; round.revealed = []; round.multiplier = 1; round.mine_positions = [];
      const available = Array.from({ length: 25 }, (_, index) => index);
      for (let index = 0; index < mines; index += 1) {
        const pick = fairInt(round.server_seed, round.client_seed, round.nonce, 'mines', available.length, index);
        round.mine_positions.push(available.splice(pick, 1)[0]);
      }
      const payload = { success: true, balance: player.crisis_coins, game: publicRound(player, round, false) };
      player.updated_at = nowUnix();
      return remember(player, common.requestId, payload);
    });
  });

  router.post('/casino/mines/reveal', limitCasinoActions, (req, res) => {
    const requestId = requireRequestId(req.body); const tile = req.body?.tile;
    if (!requestId || !Number.isSafeInteger(tile) || tile < 0 || tile > 24) return res.status(400).json({ success: false, error: 'request_id and tile 0-24 are required' });
    return sendMutation(req, res, ({ db, player }) => {
      const old = replay(player, requestId); if (old) return old;
      const round = ensureCasino(player).active.mines;
      if (!round) throw conflict('No active mines round');
      if (round.revealed.includes(tile)) throw conflict('Tile was already revealed');
      round.revealed.push(tile);
      let game;
      if (round.mine_positions.includes(tile)) game = settle(db, player, round, 'lost', 0, 'Mine revealed');
      else {
        round.multiplier = minesMultiplier(round.mines, round.revealed.length);
        if (round.revealed.length === 25 - round.mines) game = settle(db, player, round, 'won', Math.floor(round.bet * round.multiplier), 'All safe tiles revealed');
        else game = publicRound(player, round, false);
      }
      player.updated_at = nowUnix();
      return remember(player, requestId, { success: true, balance: player.crisis_coins, game });
    });
  });

  router.post('/casino/mines/cashout', limitCasinoActions, (req, res) => {
    const requestId = requireRequestId(req.body); if (!requestId) return res.status(400).json({ success: false, error: 'request_id is required' });
    return sendMutation(req, res, ({ db, player }) => {
      const old = replay(player, requestId); if (old) return old;
      const round = ensureCasino(player).active.mines;
      if (!round || !round.revealed.length) throw conflict('Reveal a safe tile before cashing out');
      const game = settle(db, player, round, 'cashed_out', Math.floor(round.bet * round.multiplier), 'Mines cash-out');
      player.updated_at = nowUnix();
      return remember(player, requestId, { success: true, balance: player.crisis_coins, game });
    });
  });

  router.post('/casino/chicken/start', limitCasinoActions, (req, res) => {
    const common = validateCommon(req.body); const risk = typeof req.body?.risk === 'string' ? req.body.risk.toLowerCase() : '';
    if (common.error) return res.status(400).json({ success: false, error: common.error });
    if (!CHICKEN_RULES[risk]) return res.status(400).json({ success: false, error: 'risk must be safe, spicy, or wild' });
    return sendMutation(req, res, ({ db, player }) => {
      const old = replay(player, common.requestId); if (old) return old;
      const round = beginRound(player, 'chicken', common.bet, common.clientSeed);
      recordWager(db, player, round);
      round.risk = risk; round.crossed = 0; round.multiplier = 1; round.caught = false;
      player.updated_at = nowUnix();
      return remember(player, common.requestId, { success: true, balance: player.crisis_coins, game: publicRound(player, round, false) });
    });
  });

  router.post('/casino/chicken/jump', limitCasinoActions, (req, res) => {
    const requestId = requireRequestId(req.body); if (!requestId) return res.status(400).json({ success: false, error: 'request_id is required' });
    return sendMutation(req, res, ({ db, player }) => {
      const old = replay(player, requestId); if (old) return old;
      const round = ensureCasino(player).active.chicken;
      if (!round) throw conflict('No active chicken round');
      const rule = CHICKEN_RULES[round.risk];
      const roll = fairInt(round.server_seed, round.client_seed, round.nonce, `chicken-${round.crossed}`, 10000);
      round.crossed += 1;
      round.caught = roll >= rule.survival;
      round.multiplier = Math.floor((rule.multiplier ** round.crossed) * 100) / 100;
      let game;
      if (round.caught) game = settle(db, player, round, 'lost', 0, 'Chicken was caught');
      else if (round.crossed >= 5) game = settle(db, player, round, 'won', Math.floor(round.bet * round.multiplier), 'All lanes crossed');
      else game = publicRound(player, round, false);
      player.updated_at = nowUnix();
      return remember(player, requestId, { success: true, balance: player.crisis_coins, game });
    });
  });

  router.post('/casino/chicken/cashout', limitCasinoActions, (req, res) => {
    const requestId = requireRequestId(req.body); if (!requestId) return res.status(400).json({ success: false, error: 'request_id is required' });
    return sendMutation(req, res, ({ db, player }) => {
      const old = replay(player, requestId); if (old) return old;
      const round = ensureCasino(player).active.chicken;
      if (!round || round.crossed < 1) throw conflict('Cross a lane before cashing out');
      const game = settle(db, player, round, 'cashed_out', Math.floor(round.bet * round.multiplier), 'Chicken cash-out');
      player.updated_at = nowUnix();
      return remember(player, requestId, { success: true, balance: player.crisis_coins, game });
    });
  });

  router.post('/casino/blackjack/start', limitCasinoActions, (req, res) => {
    const common = validateCommon(req.body); if (common.error) return res.status(400).json({ success: false, error: common.error });
    return sendMutation(req, res, ({ db, player }) => {
      const old = replay(player, common.requestId); if (old) return old;
      const round = beginRound(player, 'blackjack', common.bet, common.clientSeed);
      recordWager(db, player, round);
      const deck = shuffledDeck(round.server_seed, round.client_seed, round.nonce);
      round.player_cards = [deck.pop(), deck.pop()]; round.dealer_cards = [deck.pop(), deck.pop()]; round.deck = deck; round.message = 'Hit or stand';
      const playerTotal = handValue(round.player_cards), dealerTotal = handValue(round.dealer_cards);
      let game;
      if (playerTotal === 21 && dealerTotal === 21) game = settle(db, player, round, 'push', round.bet, 'Both have Blackjack');
      else if (playerTotal === 21) game = settle(db, player, round, 'blackjack', Math.floor(round.bet * 2.5), 'Blackjack — paid 3:2');
      else if (dealerTotal === 21) game = settle(db, player, round, 'lost', 0, 'Dealer has Blackjack');
      else game = publicRound(player, round, false);
      player.updated_at = nowUnix();
      return remember(player, common.requestId, { success: true, balance: player.crisis_coins, game });
    });
  });

  router.post('/casino/blackjack/action', limitCasinoActions, (req, res) => {
    const requestId = requireRequestId(req.body), action = req.body?.action;
    if (!requestId || !['hit', 'stand'].includes(action)) return res.status(400).json({ success: false, error: 'request_id and action hit/stand are required' });
    return sendMutation(req, res, ({ db, player }) => {
      const old = replay(player, requestId); if (old) return old;
      const round = ensureCasino(player).active.blackjack;
      if (!round) throw conflict('No active blackjack round');
      let game;
      if (action === 'hit') {
        round.player_cards.push(round.deck.pop());
        const total = handValue(round.player_cards);
        if (total > 21) game = settle(db, player, round, 'lost', 0, `Bust with ${total}`);
        else { round.message = total === 21 ? '21 — stand to resolve' : `${total} — hit or stand`; game = publicRound(player, round, false); }
      } else {
        while (handValue(round.dealer_cards) < 17) round.dealer_cards.push(round.deck.pop());
        const p = handValue(round.player_cards), d = handValue(round.dealer_cards);
        if (d > 21 || p > d) game = settle(db, player, round, 'won', round.bet * 2, `Player ${p}, dealer ${d}`);
        else if (p === d) game = settle(db, player, round, 'push', round.bet, `Push at ${p}`);
        else game = settle(db, player, round, 'lost', 0, `Player ${p}, dealer ${d}`);
      }
      player.updated_at = nowUnix();
      return remember(player, requestId, { success: true, balance: player.crisis_coins, game });
    });
  });
}

module.exports = {
  CHICKEN_RULES, CRATE_FEATURES, MAX_BET, commitment, createCasinoRoutes, ensureCasino,
  fairInt, handValue, minesMultiplier, publicFairness, rouletteColor, shuffledDeck
};
