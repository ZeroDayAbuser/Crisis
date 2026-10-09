const express = require('express');
const crypto = require('crypto');
const { isLicensePaused } = require('./lib/license-state');
const { createCasinoRoutes } = require('./casino-service');

const DISCORD_ID = /^[0-9]{17,20}$/;
const FEATURE_ID = /^[a-z0-9][a-z0-9_-]{0,63}$/;
const MAX_EVENT_LENGTH = 96;
const GAME_EVENT_ID = /^[A-Za-z0-9_-]{12,96}$/;
const GAME_MODES = new Set(['non_ranked', 'ranked', 'tournament']);
const GAMEPLAY_XP = Object.freeze({ elimination: 50, victory: 150 });
const MIN_VICTORY_SECONDS = 60;
const COIN_XP_COST = 5;
const DEFAULT_CRATE_COST = 100;
const MAX_GAME_BET = 0xffffffff;
const DAY_SECONDS = 86400;
const MAX_SUBSCRIPTION_PACK_SECONDS = 30 * DAY_SECONDS;
const SUBSCRIPTION_PACKS = Object.freeze({
  1: Object.freeze({ seconds: DAY_SECONDS, coins: 2000 }),
  7: Object.freeze({ seconds: 7 * DAY_SECONDS, coins: 12000 }),
  30: Object.freeze({ seconds: 30 * DAY_SECONDS, coins: 48000 })
});
const XP_TIME_PACKS = Object.freeze({
  3: Object.freeze({ seconds: 3 * 60 * 60, xp: 8000 }),
  24: Object.freeze({ seconds: DAY_SECONDS, xp: 40000 }),
  168: Object.freeze({ seconds: 7 * DAY_SECONDS, xp: 840000 })
});
const CRATE_FEATURES = ['premium_aim', 'custom_crosshair', 'profile_badge', 'xp_boost'];
const ROULETTE_RED = new Set([1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36]);
const ROULETTE_CHOICES = new Set(['red', 'black', 'green', 'even', 'odd', 'low', 'high']);

function nowUnix() {
  return Math.floor(Date.now() / 1000);
}


function secureRandomFloat() {
  return crypto.randomBytes(6).readUIntBE(0, 6) / 0x1000000000000;
}

function randomIndex(length, injectedRandom) {
  if (!Number.isSafeInteger(length) || length < 1) throw new RangeError('length must be positive');
  if (!injectedRandom) return crypto.randomInt(length);
  const value = Number(injectedRandom());
  return Math.max(0, Math.min(length - 1, Math.floor((Number.isFinite(value) ? value : 0) * length)));
}

function secretMatches(expected, supplied) {
  if (typeof expected !== 'string' || expected.length < 32 || typeof supplied !== 'string') return false;
  const expectedHash = crypto.createHash('sha256').update(expected).digest();
  const suppliedHash = crypto.createHash('sha256').update(supplied).digest();
  return crypto.timingSafeEqual(expectedHash, suppliedHash);
}

function createPlayer(discordId, profile = {}) {
  return {
    discord_id: discordId,
    discord_profile: {
      username: typeof profile.username === 'string' ? profile.username.slice(0, 64) : '',
      display_name: typeof profile.display_name === 'string' ? profile.display_name.slice(0, 64) : '',
      avatar_url: typeof profile.avatar_url === 'string' ? profile.avatar_url.slice(0, 512) : ''
    },
    gameplay: {
      games_played: 0, wins: 0, kills: 0, deaths: 0,
      ranked_games: 0, non_ranked_games: 0, tournament_games: 0,
      ranked_kills: 0, non_ranked_kills: 0, tournament_kills: 0,
      accounts_used: 1, total_game_seconds: 0,
      active_match: null, processed_events: []
    },
    role: 'Member',
    xp_balance: 0,
    lifetime_xp: 0,
    crisis_coins: 0,
    recent_xp_earnings: [],
    lifetime_xp_progress: [],
    unlocked_features: [],
    blackjack: null,
    last_roulette: null,
    created_at: nowUnix(),
    updated_at: nowUnix()
  };
}

function normalizePlayer(player, discordId) {
  const fresh = createPlayer(discordId);
  if (!player || typeof player !== 'object' || Array.isArray(player)) return fresh;
  const result = { ...fresh, ...player, discord_id: discordId };
  result.discord_profile = { ...fresh.discord_profile, ...(player.discord_profile || {}) };
  result.role = typeof player.role === 'string' && player.role.trim() ? player.role.trim().slice(0, 48) : fresh.role;
  result.gameplay = { ...fresh.gameplay, ...(player.gameplay || {}) };
  result.recent_xp_earnings = Array.isArray(player.recent_xp_earnings) ? player.recent_xp_earnings.slice(-7) : [];
  result.lifetime_xp_progress = Array.isArray(player.lifetime_xp_progress) ? player.lifetime_xp_progress : [];
  result.unlocked_features = Array.isArray(player.unlocked_features)
    ? [...new Set(player.unlocked_features.filter((value) => typeof value === 'string' && FEATURE_ID.test(value)))] : [];
  result.blackjack = player.blackjack && typeof player.blackjack === 'object' && !Array.isArray(player.blackjack)
    ? player.blackjack : null;
  result.last_roulette = player.last_roulette && typeof player.last_roulette === 'object' && !Array.isArray(player.last_roulette)
    ? player.last_roulette : null;
  for (const field of ['games_played', 'wins', 'kills', 'deaths', 'ranked_games', 'non_ranked_games',
    'tournament_games', 'ranked_kills', 'non_ranked_kills', 'tournament_kills', 'accounts_used', 'total_game_seconds']) {
    if (!Number.isSafeInteger(result.gameplay[field]) || result.gameplay[field] < 0) result.gameplay[field] = field === 'accounts_used' ? 1 : 0;
  }
  result.gameplay.wins = Math.min(result.gameplay.wins, result.gameplay.games_played);
  result.gameplay.processed_events = Array.isArray(result.gameplay.processed_events)
    ? result.gameplay.processed_events.filter((value) => typeof value === 'string' && GAME_EVENT_ID.test(value)).slice(-256) : [];
  result.gameplay.active_match = result.gameplay.active_match && typeof result.gameplay.active_match === 'object'
    && GAME_EVENT_ID.test(result.gameplay.active_match.id || '') && GAME_MODES.has(result.gameplay.active_match.mode)
    ? result.gameplay.active_match : null;
  for (const field of ['xp_balance', 'lifetime_xp', 'crisis_coins']) {
    if (!Number.isSafeInteger(result[field]) || result[field] < 0) result[field] = 0;
  }
  return result;
}

function levelProgress(lifetimeXp) {
  let level = 1;
  let levelStartXp = 0;
  let levelRequirement = 1000;
  while (lifetimeXp >= levelStartXp + levelRequirement) {
    levelStartXp += levelRequirement;
    levelRequirement += 500;
    level += 1;
  }
  return {
    level,
    level_start_xp: levelStartXp,
    next_level_xp: levelStartXp + levelRequirement,
    xp_into_level: lifetimeXp - levelStartXp,
    xp_required: levelRequirement,
    progress: levelRequirement ? (lifetimeXp - levelStartXp) / levelRequirement : 0
  };
}

function publicGameplay(player) {
  const game = player.gameplay;
  const games = game.games_played;
  return {
    games_played: games, wins: game.wins, kills: game.kills, deaths: game.deaths,
    ranked_games: game.ranked_games, non_ranked_games: game.non_ranked_games,
    tournament_games: game.tournament_games, ranked_kills: game.ranked_kills,
    non_ranked_kills: game.non_ranked_kills, tournament_kills: game.tournament_kills,
    accounts_used: game.accounts_used,
    average_game_minutes: games ? game.total_game_seconds / games / 60 : 0,
    winrate: games ? game.wins * 100 / games : 0,
    kd: game.deaths ? game.kills / game.deaths : game.kills
  };
}

function ensurePlayer(db, discordId, profile) {
  if (!db.players || typeof db.players !== 'object' || Array.isArray(db.players)) db.players = {};
  const player = normalizePlayer(db.players[discordId], discordId);
  if (profile) {
    for (const field of ['username', 'display_name', 'avatar_url']) {
      if (typeof profile[field] === 'string' && profile[field]) {
        const max = field === 'avatar_url' ? 512 : 64;
        player.discord_profile[field] = profile[field].slice(0, max);
      }
    }
  }
  player.updated_at = nowUnix();
  db.players[discordId] = player;
  return player;
}

function authenticate(db, key, hwid, current = nowUnix()) {
  if (typeof key !== 'string' || typeof hwid !== 'string' || !db || !db.keys || !db.keys[key]) return null;
  const license = db.keys[key];
  if (isLicensePaused(db, license)) return null;
  if (license.hwid !== hwid || !Number.isSafeInteger(license.expiry_unix) || current > license.expiry_unix) return null;
  if (typeof license.discord_id !== 'string' || !DISCORD_ID.test(license.discord_id)) return null;
  return { license, player: ensurePlayer(db, license.discord_id) };
}

function publicPlayer(player) {
  let blackjack = null;
  if (player.blackjack) {
    blackjack = {
      bet: player.blackjack.bet,
      player_cards: player.blackjack.player_cards,
      dealer_cards: player.blackjack.status === 'playing'
        ? [player.blackjack.dealer_cards[0], '??'] : player.blackjack.dealer_cards,
      player_total: handValue(player.blackjack.player_cards),
      dealer_total: player.blackjack.status === 'playing' ? null : handValue(player.blackjack.dealer_cards),
      status: player.blackjack.status,
      payout: player.blackjack.payout || 0,
      message: player.blackjack.message || ''
    };
  }
  return {
    discord_id: player.discord_id,
    discord_profile: player.discord_profile,
    role: player.role,
    stats: publicGameplay(player),
    level: levelProgress(player.lifetime_xp),
    xp_balance: player.xp_balance,
    lifetime_xp: player.lifetime_xp,
    crisis_coins: player.crisis_coins,
    recent_xp_earnings: player.recent_xp_earnings,
    lifetime_xp_progress: player.lifetime_xp_progress.slice(-24),
    xp_rates: GAMEPLAY_XP,
    unlocked_features: player.unlocked_features,
    blackjack,
    last_roulette: player.last_roulette
  };
}

function shuffledDeck(random) {
  const suits = ['S', 'H', 'D', 'C'];
  const ranks = ['A', '2', '3', '4', '5', '6', '7', '8', '9', '10', 'J', 'Q', 'K'];
  const deck = [];
  for (const suit of suits) for (const rank of ranks) deck.push(`${rank}${suit}`);
  for (let index = deck.length - 1; index > 0; --index) {
    const swap = randomIndex(index + 1, random);
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

function settleBlackjack(player, status, payout, message) {
  player.crisis_coins += payout;
  player.blackjack.status = status;
  player.blackjack.payout = payout;
  player.blackjack.message = message;
}

function rouletteColor(number) {
  if (number === 0) return 'green';
  return ROULETTE_RED.has(number) ? 'red' : 'black';
}

function rouletteWon(number, color, choice) {
  if (choice === color) return true;
  if (number === 0) return false;
  if (choice === 'even') return number % 2 === 0;
  if (choice === 'odd') return number % 2 === 1;
  if (choice === 'low') return number <= 18;
  if (choice === 'high') return number >= 19;
  return false;
}

function scoreboard(db, discordId, radius = 3) {
  const players = Object.entries(db.players || {}).map(([id, value]) => normalizePlayer(value, id));
  players.sort((left, right) => right.lifetime_xp - left.lifetime_xp || left.discord_id.localeCompare(right.discord_id));
  const index = players.findIndex((player) => player.discord_id === discordId);
  if (index < 0) return { placement: null, total_players: players.length, entries: [] };
  const start = Math.max(0, Math.min(index - radius, Math.max(0, players.length - (radius * 2 + 1))));
  return {
    placement: index + 1,
    total_players: players.length,
    entries: players.slice(start, start + radius * 2 + 1).map((player, offset) => ({
      placement: start + offset + 1,
      discord_id: player.discord_id,
      display_name: player.discord_profile.display_name || player.discord_profile.username || `Player ${player.discord_id.slice(-4)}`,
      lifetime_xp: player.lifetime_xp,
      level: levelProgress(player.lifetime_xp).level,
      current_user: player.discord_id === discordId
    }))
  };
}

function awardXp(player, amount, event, context = '') {
  const earning = { amount, event, context, earned_at: nowUnix() };
  player.xp_balance += amount;
  player.lifetime_xp += amount;
  player.recent_xp_earnings.push(earning);
  player.recent_xp_earnings = player.recent_xp_earnings.slice(-7);
  player.lifetime_xp_progress.push({ ...earning, lifetime_xp: player.lifetime_xp });
  player.lifetime_xp_progress = player.lifetime_xp_progress.slice(-256);
  player.updated_at = earning.earned_at;
  return earning;
}

function rememberGameplayEvent(player, eventId) {
  player.gameplay.processed_events.push(eventId);
  player.gameplay.processed_events = player.gameplay.processed_events.slice(-256);
}

function responsePayload(db, player) {
  const board = scoreboard(db, player.discord_id);
  return { success: true, player: publicPlayer(player), scoreboard: board, leaderboard: board };
}

function bindPlayerToLicense(db, key, discordId, profile) {
  const license = db.keys && db.keys[key];
  if (!license || !DISCORD_ID.test(discordId || '')) return { error: 'A valid Discord profile is required' };
  if (license.discord_id && license.discord_id !== discordId) return { error: 'License is already linked to another Discord profile' };
  license.discord_id = discordId;
  return { player: ensurePlayer(db, discordId, profile) };
}

function authenticateRoute(options, req, db) {
  if (typeof options.authenticateAccess !== 'function') return null;
  const access = options.authenticateAccess(req, db);
  const license = access?.license;
  if (!license || typeof license.discord_id !== 'string' || !DISCORD_ID.test(license.discord_id)) return null;
  return { ...access, player: ensurePlayer(db, license.discord_id) };
}

function mountPlayerRoutes(app, options) {
  const router = express.Router();
  const casinoHits = new Map();
  const casinoWindowMs = Math.max(1000, Number(options.casinoRateLimit?.windowMs) || 10000);
  const casinoMaxActions = Math.max(1, Number(options.casinoRateLimit?.max) || 30);
  const readDatabase = () => (options.store ? options.store.read() : options.loadDatabase());


  function commitPlayerMutation(req, mutate) {
    if (options.store) {
      return options.store.transaction((db) => {
        const auth = authenticateRoute(options, req, db);
        if (!auth) {
          const error = new Error('Unauthorized');
          error.status = 401;
          throw error;
        }
        const context = { db, ...auth };
        req.playerContext = context;
        return mutate(context);
      });
    }

    const payload = mutate(req.playerContext);
    if (!options.saveDatabase(req.playerContext.db)) {
      const error = new Error('Failed to save database');
      error.status = 500;
      throw error;
    }
    return payload;
  }

  function sendMutation(req, res, mutate) {
    try {
      return res.json(commitPlayerMutation(req, mutate));
    } catch (error) {
      if (error.status === 401) return res.status(401).json({ success: false, error: 'Unauthorized' });
      if (error.status === 409) return res.status(409).json({ success: false, error: error.message, ...error.extra });
      if (error.status === 500) return res.status(500).json({ success: false, error: error.message });
      throw error;
    }
  }

  router.use((req, res, next) => {
    const db = readDatabase();
    const auth = authenticateRoute(options, req, db);
    if (!auth) return res.status(401).json({ success: false, error: 'Unauthorized' });
    req.playerContext = { db, ...auth };
    next();
  });

  const requireTrustedEvent = (req, res, next) => {
    if (typeof options.eventSecret !== 'string' || options.eventSecret.length < 32)
      return res.status(503).json({ success: false, error: 'XP awarding is not configured' });
    if (!secretMatches(options.eventSecret, req.get('x-crisis-event-secret')))
      return res.status(403).json({ success: false, error: 'Trusted event authorization required' });
    next();
  };

  const limitCasinoActions = (req, res, next) => {
    const now = Date.now();
    const key = req.playerContext.player.discord_id;
    const recent = (casinoHits.get(key) || []).filter((timestamp) => now - timestamp < casinoWindowMs);
    if (recent.length >= casinoMaxActions) {
      const retryMs = casinoWindowMs - (now - recent[0]);
      res.set('Retry-After', String(Math.max(1, Math.ceil(retryMs / 1000))));
      return res.status(429).json({ success: false, error: 'Too many casino actions; try again shortly' });
    }
    recent.push(now);
    casinoHits.set(key, recent);
    next();
  };

  router.get('/', (req, res) => res.json({
    ...responsePayload(req.playerContext.db, req.playerContext.player),
    subscription: {
      expires_unix: req.playerContext.license.expiry_unix,
      remaining_seconds: Math.max(0, req.playerContext.license.expiry_unix - nowUnix())
    }
  }));
  router.get('/scoreboard', (req, res) => res.json({ success: true, scoreboard: scoreboard(req.playerContext.db, req.playerContext.player.discord_id) }));


  router.all('/stats', (req, res) => res.status(410).json({
    success: false, error: 'Profile statistics are server-authoritative'
  }));

  router.post('/gameplay/match/start', (req, res) => {
    const matchId = req.body?.match_id;
    const requestId = req.body?.request_id;
    const mode = req.body?.mode || 'non_ranked';
    if (!GAME_EVENT_ID.test(matchId || '') || !GAME_EVENT_ID.test(requestId || '') || !GAME_MODES.has(mode))
      return res.status(400).json({ success: false, error: 'match_id, request_id, and a valid mode are required' });
    return sendMutation(req, res, ({ db, player }) => {
      if (player.gameplay.processed_events.includes(requestId))
        return { ...responsePayload(db, player), duplicate: true };
      const active = player.gameplay.active_match;
      if (active && active.id === matchId) {
        rememberGameplayEvent(player, requestId);
        return { ...responsePayload(db, player), duplicate: true,
          match: { id: active.id, mode: active.mode, kills: active.kills, status: 'active' } };
      }
      if (active && active.id !== matchId && nowUnix() - active.started_at < 6 * 60 * 60) {
        const error = new Error('Finish the active match first'); error.status = 409; throw error;
      }
      player.gameplay.active_match = {
        id: matchId, mode, started_at: nowUnix(), kills: 0
      };
      rememberGameplayEvent(player, requestId);
      player.updated_at = nowUnix();
      return { ...responsePayload(db, player), match: { id: matchId, mode, status: 'active' } };
    });
  });

  router.post('/gameplay/elimination', (req, res) => {
    const matchId = req.body?.match_id;
    const eventId = req.body?.event_id;
    if (!GAME_EVENT_ID.test(matchId || '') || !GAME_EVENT_ID.test(eventId || ''))
      return res.status(400).json({ success: false, error: 'match_id and event_id are required' });
    return sendMutation(req, res, ({ db, player }) => {
      if (player.gameplay.processed_events.includes(eventId))
        return { ...responsePayload(db, player), duplicate: true };
      const match = player.gameplay.active_match;
      if (!match || match.id !== matchId) {
        const error = new Error('No matching active game'); error.status = 409; throw error;
      }
      if (match.kills >= 50) {
        const error = new Error('Elimination limit reached for this match'); error.status = 409; throw error;
      }
      match.kills += 1;
      player.gameplay.kills += 1;
      player.gameplay[`${match.mode}_kills`] += 1;
      rememberGameplayEvent(player, eventId);
      const earning = awardXp(player, GAMEPLAY_XP.elimination, 'Elimination', match.mode);
      return { ...responsePayload(db, player), earning, match: { id: match.id, mode: match.mode, kills: match.kills, status: 'active' } };
    });
  });

  router.post('/gameplay/match/end', (req, res) => {
    const matchId = req.body?.match_id;
    const eventId = req.body?.event_id;
    const won = req.body?.won;
    if (!GAME_EVENT_ID.test(matchId || '') || !GAME_EVENT_ID.test(eventId || '') || typeof won !== 'boolean')
      return res.status(400).json({ success: false, error: 'match_id, event_id, and won are required' });
    return sendMutation(req, res, ({ db, player }) => {
      if (player.gameplay.processed_events.includes(eventId))
        return { ...responsePayload(db, player), duplicate: true };
      const match = player.gameplay.active_match;
      if (!match || match.id !== matchId) {
        const error = new Error('No matching active game'); error.status = 409; throw error;
      }
      const duration = Math.max(0, nowUnix() - match.started_at);
      if (won && duration < MIN_VICTORY_SECONDS) {
        const error = new Error('A victory cannot settle before the minimum match duration');
        error.status = 409; throw error;
      }
      player.gameplay.games_played += 1;
      player.gameplay[`${match.mode}_games`] += 1;
      player.gameplay.total_game_seconds += Math.min(duration, 6 * 60 * 60);
      if (won) player.gameplay.wins += 1;
      else player.gameplay.deaths += 1;
      player.gameplay.active_match = null;
      rememberGameplayEvent(player, eventId);
      const earning = won ? awardXp(player, GAMEPLAY_XP.victory, 'Victory Royale', match.mode) : null;
      player.updated_at = nowUnix();
      return { ...responsePayload(db, player), earning, match: { id: match.id, mode: match.mode, kills: match.kills, won, status: 'complete' } };
    });
  });

  router.post('/xp/earn', requireTrustedEvent, (req, res) => {
    const amount = req.body?.amount;
    const event = req.body?.event;
    if (!Number.isSafeInteger(amount) || amount < 1 || amount > 1000000 || typeof event !== 'string' || !event.trim() || event.length > MAX_EVENT_LENGTH)
      return res.status(400).json({ success: false, error: 'amount and event are required' });
    return sendMutation(req, res, ({ db, player }) => {
      awardXp(player, amount, event.trim(), 'trusted-service');
      return responsePayload(db, player);
    });
  });

  router.post('/coins/purchase', (req, res) => {
    const coins = req.body?.coins;
    if (!Number.isSafeInteger(coins) || coins < 1 || coins > 1000000)
      return res.status(400).json({ success: false, error: 'coins must be a positive integer' });
    const cost = coins * COIN_XP_COST;
    return sendMutation(req, res, ({ db, player }) => {
      if (!Number.isSafeInteger(cost) || player.xp_balance < cost) {
        const error = new Error('Insufficient XP balance');
        error.status = 409;
        error.extra = { xp_cost: cost };
        throw error;
      }
      player.xp_balance -= cost;
      const previousCoins = player.crisis_coins;
      player.crisis_coins += coins;
      player.updated_at = nowUnix();
      if (!Array.isArray(db.coin_ledger)) db.coin_ledger = [];
      db.coin_ledger.push({ id: crypto.randomUUID(), discord_id: player.discord_id, delta: coins, previous: previousCoins, balance: player.crisis_coins, reason: 'XP exchange', actor_role: 'user', created_at: player.updated_at });
      return { ...responsePayload(db, player), xp_spent: cost, coins_purchased: coins };
    });
  });

  router.post('/subscription/redeem', (req, res) => {
    const days = req.body?.days;
    const pack = SUBSCRIPTION_PACKS[days];
    if (!Number.isSafeInteger(days) || !pack)
      return res.status(400).json({ success: false, error: 'days must be one of: 1, 7, 30' });

    return sendMutation(req, res, ({ db, license, player }) => {
      const now = nowUnix();
      const currentExpiry = Number.isSafeInteger(license.expiry_unix) ? license.expiry_unix : now;
      const secondsAdded = pack.seconds;
      const coinsSpent = pack.coins;
      if (player.crisis_coins < coinsSpent) {
        const error = new Error('Insufficient Crisis Coins');
        error.status = 409;
        error.extra = { coins_required: coinsSpent };
        throw error;
      }
      if (!Number.isSafeInteger(currentExpiry + secondsAdded)) {
        const error = new Error('Subscription expiry cannot be extended further');
        error.status = 409;
        throw error;
      }
      const previousCoins = player.crisis_coins;
      player.crisis_coins -= coinsSpent;
      license.expiry_unix = currentExpiry + secondsAdded;
      player.updated_at = now;
      if (!Array.isArray(db.coin_ledger)) db.coin_ledger = [];
      if (!Array.isArray(db.subscription_ledger)) db.subscription_ledger = [];
      db.coin_ledger.push({ id: crypto.randomUUID(), discord_id: player.discord_id, delta: -coinsSpent, previous: previousCoins, balance: player.crisis_coins, reason: `Subscription redemption (${days} days)`, actor_role: 'user', created_at: now });
      db.subscription_ledger.push({ id: crypto.randomUUID(), discord_id: player.discord_id, seconds: secondsAdded, previous: currentExpiry, expiry: license.expiry_unix, reason: `Subscription redemption (${days} days)`, actor_role: 'user', created_at: now });
      return {
        ...responsePayload(db, player),
        expiry_unix: license.expiry_unix,
        remaining_seconds: license.expiry_unix - now,
        days_requested: days,
        seconds_added: secondsAdded,
        coins_spent: coinsSpent
      };
    });
  });

  router.post('/subscription/redeem-xp', (req, res) => {
    const hours = req.body?.hours;
    const pack = XP_TIME_PACKS[hours];
    if (!Number.isSafeInteger(hours) || !pack)
      return res.status(400).json({ success: false, error: 'hours must be one of: 3, 24, 168' });
    return sendMutation(req, res, ({ db, license, player }) => {
      if (player.xp_balance < pack.xp) {
        const error = new Error('Insufficient XP balance'); error.status = 409;
        error.extra = { xp_required: pack.xp }; throw error;
      }
      const now = nowUnix();
      const previousExpiry = Math.max(now, Number.isSafeInteger(license.expiry_unix) ? license.expiry_unix : now);
      if (!Number.isSafeInteger(previousExpiry + pack.seconds)) {
        const error = new Error('Subscription expiry cannot be extended further'); error.status = 409; throw error;
      }
      const previousXp = player.xp_balance;
      player.xp_balance -= pack.xp;
      license.expiry_unix = previousExpiry + pack.seconds;
      player.updated_at = now;
      if (!Array.isArray(db.xp_ledger)) db.xp_ledger = [];
      if (!Array.isArray(db.subscription_ledger)) db.subscription_ledger = [];
      db.xp_ledger.push({ id: crypto.randomUUID(), discord_id: player.discord_id, delta: -pack.xp,
        previous: previousXp, balance: player.xp_balance, reason: `Subscription extension (${hours} hours)`, created_at: now });
      db.subscription_ledger.push({ id: crypto.randomUUID(), discord_id: player.discord_id, seconds: pack.seconds,
        previous: previousExpiry, expiry: license.expiry_unix, reason: `XP redemption (${hours} hours)`, actor_role: 'user', created_at: now });
      return { ...responsePayload(db, player), expiry_unix: license.expiry_unix,
        remaining_seconds: license.expiry_unix - now, hours, seconds_added: pack.seconds, xp_spent: pack.xp };
    });
  });


  router.use(['/gamble', '/games/roulette', '/games/blackjack/start', '/games/blackjack/action', '/crates/open'], (req, res) =>
    res.status(410).json({ success: false, error: 'Legacy casino route retired; update the client to /v1/player/casino/*' }));

  router.post('/gamble', limitCasinoActions, (req, res) => {
    const bet = req.body?.bet;
    if (!Number.isSafeInteger(bet) || bet < 1) return res.status(400).json({ success: false, error: 'bet must be a positive integer' });
    return sendMutation(req, res, ({ db, player }) => {
      if (player.crisis_coins < bet) {
        const error = new Error('Insufficient Crisis Coins');
        error.status = 409;
        throw error;
      }
      const won = (options.random || secureRandomFloat)() < 0.45;
      player.crisis_coins += won ? bet : -bet;
      player.updated_at = nowUnix();
      return { ...responsePayload(db, player), won, coin_change: won ? bet : -bet };
    });
  });

  router.post('/games/roulette', limitCasinoActions, (req, res) => {
    const bet = req.body?.bet;
    const choice = typeof req.body?.choice === 'string' ? req.body.choice.toLowerCase() : '';
    if (!Number.isSafeInteger(bet) || bet < 1 || bet > MAX_GAME_BET)
      return res.status(400).json({ success: false, error: 'bet must be a positive integer' });
    if (!ROULETTE_CHOICES.has(choice))
      return res.status(400).json({ success: false, error: 'Invalid roulette choice' });
    return sendMutation(req, res, ({ db, player }) => {
      if (player.crisis_coins < bet) {
        const error = new Error('Insufficient Crisis Coins');
        error.status = 409;
        throw error;
      }
      const number = randomIndex(37, options.random);
      const color = rouletteColor(number);
      const won = rouletteWon(number, color, choice);
      const multiplier = choice === 'green' ? 36 : 2;
      const payout = won ? bet * multiplier : 0;
      player.crisis_coins = player.crisis_coins - bet + payout;
      player.last_roulette = {
        number, color, choice, bet, won, payout, coin_change: payout - bet, played_at: nowUnix()
      };
      player.updated_at = nowUnix();
      return { ...responsePayload(db, player), game: player.last_roulette };
    });
  });

  router.post('/games/blackjack/start', limitCasinoActions, (req, res) => {
    const bet = req.body?.bet;
    if (!Number.isSafeInteger(bet) || bet < 1 || bet > MAX_GAME_BET)
      return res.status(400).json({ success: false, error: 'Blackjack bet must be a positive integer' });
    return sendMutation(req, res, ({ db, player }) => {
      if (player.blackjack?.status === 'playing') {
        const error = new Error('Finish the current Blackjack hand first');
        error.status = 409;
        throw error;
      }
      if (player.crisis_coins < bet) {
        const error = new Error('Insufficient Crisis Coins');
        error.status = 409;
        throw error;
      }
      const deck = shuffledDeck(options.random);
      player.crisis_coins -= bet;
      player.blackjack = {
        bet,
        player_cards: [deck.pop(), deck.pop()],
        dealer_cards: [deck.pop(), deck.pop()],
        deck,
        status: 'playing',
        payout: 0,
        message: 'Hit or stand',
        started_at: nowUnix()
      };
      const playerTotal = handValue(player.blackjack.player_cards);
      const dealerTotal = handValue(player.blackjack.dealer_cards);
      if (playerTotal === 21) {
        if (dealerTotal === 21) settleBlackjack(player, 'push', bet, 'Both have Blackjack — push');
        else settleBlackjack(player, 'blackjack', Math.floor(bet * 2.5), 'Blackjack! Paid 3:2');
      } else if (dealerTotal === 21) {
        settleBlackjack(player, 'lost', 0, 'Dealer has Blackjack');
      }
      player.updated_at = nowUnix();
      return responsePayload(db, player);
    });
  });

  router.post('/games/blackjack/action', limitCasinoActions, (req, res) => {
    const action = req.body?.action;
    if (!['hit', 'stand'].includes(action)) return res.status(400).json({ success: false, error: 'action must be hit or stand' });
    return sendMutation(req, res, ({ db, player }) => {
      const game = player.blackjack;
      if (!game || game.status !== 'playing') {
        const error = new Error('No active Blackjack hand');
        error.status = 409;
        throw error;
      }
      if (action === 'hit') {
        game.player_cards.push(game.deck.pop());
        const total = handValue(game.player_cards);
        if (total > 21) settleBlackjack(player, 'lost', 0, `Bust with ${total}`);
        else if (total === 21) game.message = '21 — stand to resolve';
        else game.message = `${total} — hit or stand`;
      } else {
        while (handValue(game.dealer_cards) < 17) game.dealer_cards.push(game.deck.pop());
        const playerTotal = handValue(game.player_cards);
        const dealerTotal = handValue(game.dealer_cards);
        if (dealerTotal > 21 || playerTotal > dealerTotal) settleBlackjack(player, 'won', game.bet * 2, `You win ${playerTotal} to ${dealerTotal}`);
        else if (playerTotal === dealerTotal) settleBlackjack(player, 'push', game.bet, `Push at ${playerTotal}`);
        else settleBlackjack(player, 'lost', 0, `Dealer wins ${dealerTotal} to ${playerTotal}`);
      }
      player.updated_at = nowUnix();
      return responsePayload(db, player);
    });
  });

  createCasinoRoutes(router, { limitCasinoActions, sendMutation });

  router.post('/crates/open', (req, res) => {
    const cost = req.body?.cost === undefined ? DEFAULT_CRATE_COST : req.body.cost;
    if (!Number.isSafeInteger(cost) || cost < 1 || cost > 1000000) return res.status(400).json({ success: false, error: 'Invalid crate cost' });
    return sendMutation(req, res, ({ db, player }) => {
      if (player.crisis_coins < cost) {
        const error = new Error('Insufficient Crisis Coins');
        error.status = 409;
        throw error;
      }
      player.crisis_coins -= cost;
      const locked = CRATE_FEATURES.filter((feature) => !player.unlocked_features.includes(feature));
      const unlocked_feature = locked.length ? locked[randomIndex(locked.length, options.random)] : null;
      if (unlocked_feature) player.unlocked_features.push(unlocked_feature);
      player.updated_at = nowUnix();
      return { ...responsePayload(db, player), unlocked_feature, coins_spent: cost };
    });
  });

  router.get('/features/:feature', (req, res) => {
    if (!FEATURE_ID.test(req.params.feature)) return res.status(400).json({ success: false, error: 'Invalid feature id' });
    res.json({ success: true, feature: req.params.feature, unlocked: req.playerContext.player.unlocked_features.includes(req.params.feature) });
  });

  const httpRouter = express.Router();
  httpRouter.use(express.json({ limit: '32kb', strict: true }));
  httpRouter.use(router);
  app.use('/v1/player', httpRouter);
  return router;
}

module.exports = {
  COIN_XP_COST,
  DAY_SECONDS,
  MAX_SUBSCRIPTION_PACK_SECONDS,
  SUBSCRIPTION_PACKS,
  XP_TIME_PACKS,
  authenticate,
  bindPlayerToLicense,
  createPlayer,
  ensurePlayer,
  levelProgress,
  mountPlayerRoutes,
  normalizePlayer,
  publicPlayer,
  publicGameplay,
  handValue,
  rouletteColor,
  rouletteWon,
  secureRandomFloat,
  secretMatches,
  shuffledDeck,
  responsePayload,
  scoreboard
};
