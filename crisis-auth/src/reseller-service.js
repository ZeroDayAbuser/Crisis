const crypto = require('crypto');
const { parseGameSelection } = require('./lib/game-entitlements');

const MAX_ORDER_QUANTITY = 1000;
const MAX_BALANCE_CENTS = 100_000_000;

const PRODUCTS = Object.freeze({
  one_day: Object.freeze({ label: '1 Day', durationSeconds: 86400 }),
  three_days: Object.freeze({ label: '3 Day', durationSeconds: 3 * 86400 }),
  one_week: Object.freeze({ label: '1 Week', durationSeconds: 7 * 86400 }),
  one_month: Object.freeze({ label: '1 Month', durationSeconds: 30 * 86400 })
});

const TIERS = Object.freeze([
  Object.freeze({ minimum: 100, label: '100+', discountPercent: 50, prices: Object.freeze({ one_day: 450, three_days: 750, one_week: 1750, one_month: 3000 }) }),
  Object.freeze({ minimum: 50, label: '50–99', discountPercent: 45, prices: Object.freeze({ one_day: 494, three_days: 824, one_week: 1924, one_month: 3299 }) }),
  Object.freeze({ minimum: 25, label: '25–49', discountPercent: 40, prices: Object.freeze({ one_day: 539, three_days: 899, one_week: 2099, one_month: 3599 }) }),
  Object.freeze({ minimum: 10, label: '10–24', discountPercent: 35, prices: Object.freeze({ one_day: 584, three_days: 974, one_week: 2274, one_month: 3899 }) }),
  Object.freeze({ minimum: 5, label: '5–9', discountPercent: 30, prices: Object.freeze({ one_day: 629, three_days: 1049, one_week: 2449, one_month: 4199 }) }),
  Object.freeze({ minimum: 1, label: '1–4', discountPercent: 25, prices: Object.freeze({ one_day: 674, three_days: 1124, one_week: 2624, one_month: 4499 }) })
]);

function nowUnix() { return Math.floor(Date.now() / 1000); }
function validDiscordId(value) { return /^\d{17,20}$/.test(String(value || '')); }
function formatMoney(cents) { return `$${(cents / 100).toFixed(2)}`; }

function centsFromAmount(value) {
  const amount = Number(value);
  if (!Number.isFinite(amount) || amount < 0) return null;
  const cents = Math.round(amount * 100);
  if (!Number.isSafeInteger(cents) || cents > MAX_BALANCE_CENTS || Math.abs(amount * 100 - cents) > 1e-7) return null;
  return cents;
}

function ensureCollections(db) {
  if (!db.resellers || typeof db.resellers !== 'object' || Array.isArray(db.resellers)) db.resellers = {};
  if (!Array.isArray(db.reseller_ledger)) db.reseller_ledger = [];
}

function resellerAccount(db, discordId) {
  ensureCollections(db);
  const account = db.resellers[String(discordId || '')];
  if (!account || account.status !== 'active' || !Number.isSafeInteger(account.balance_cents) || account.balance_cents < 0) return null;
  return account;
}

function addReseller(db, discordId, startingBalanceCents, actorId, current = nowUnix()) {
  ensureCollections(db);
  if (!validDiscordId(discordId)) throw new Error('A valid Discord user is required.');
  if (!Number.isSafeInteger(startingBalanceCents) || startingBalanceCents < 0 || startingBalanceCents > MAX_BALANCE_CENTS) throw new Error('The starting balance is invalid.');
  if (db.resellers[discordId]?.status === 'active') throw new Error('That Discord user is already a reseller.');

  const account = {
    discord_id: discordId,
    status: 'active',
    balance_cents: startingBalanceCents,
    total_spent_cents: 0,
    total_keys_generated: 0,
    created_at: current,
    created_by: actorId,
    updated_at: current
  };
  db.resellers[discordId] = account;
  db.reseller_ledger.push({
    id: crypto.randomUUID(), reseller_discord_id: discordId, type: 'account_created',
    delta_cents: startingBalanceCents, balance_before_cents: 0, balance_after_cents: startingBalanceCents,
    actor_discord_id: actorId, reason: 'Reseller account created', created_at: current
  });
  return account;
}

function adjustResellerBalance(db, discordId, action, amountCents, actorId, reason, current = nowUnix()) {
  const account = resellerAccount(db, discordId);
  if (!account) throw new Error('That Discord user does not have an active reseller account.');
  if (!['add', 'set', 'remove'].includes(action)) throw new Error('Choose add, set, or remove.');
  if (!Number.isSafeInteger(amountCents) || amountCents < 0 || amountCents > MAX_BALANCE_CENTS) throw new Error('The balance amount is invalid.');
  if (action !== 'set' && amountCents === 0) throw new Error('The balance adjustment must be greater than zero.');

  const before = account.balance_cents;
  const after = action === 'add' ? before + amountCents : action === 'remove' ? before - amountCents : amountCents;
  if (!Number.isSafeInteger(after) || after < 0 || after > MAX_BALANCE_CENTS) throw new Error('The resulting reseller balance is outside the allowed range.');

  account.balance_cents = after;
  account.updated_at = current;
  db.reseller_ledger.push({
    id: crypto.randomUUID(), reseller_discord_id: discordId, type: `balance_${action}`,
    delta_cents: after - before, balance_before_cents: before, balance_after_cents: after,
    actor_discord_id: actorId, reason, created_at: current
  });
  return { account, before, after };
}

function quoteOrder(productId, quantity) {
  const product = PRODUCTS[productId];
  if (!product) throw new Error('Choose a valid key duration.');
  if (!Number.isSafeInteger(quantity) || quantity < 1 || quantity > MAX_ORDER_QUANTITY) throw new Error(`Quantity must be between 1 and ${MAX_ORDER_QUANTITY}.`);
  const tier = TIERS.find((candidate) => quantity >= candidate.minimum);
  const unitPriceCents = tier.prices[productId];
  return {
    productId, productLabel: product.label, durationSeconds: product.durationSeconds,
    quantity, tierLabel: tier.label, discountPercent: tier.discountPercent,
    unitPriceCents, totalCents: unitPriceCents * quantity
  };
}

function generateResellerOrder(db, discordId, productId, quantity, games, generateKey, current = nowUnix()) {
  const account = resellerAccount(db, discordId);
  if (!account) throw new Error('You do not have an active reseller account.');
  const quote = quoteOrder(productId, quantity);
  const entitledGames = parseGameSelection(games);
  if (!entitledGames) throw new Error('Choose at least one valid game.');
  if (account.balance_cents < quote.totalCents) {
    throw new Error(`Insufficient reseller balance. Required ${formatMoney(quote.totalCents)}; available ${formatMoney(account.balance_cents)}.`);
  }

  const orderId = crypto.randomUUID();
  const keys = [];
  for (let index = 0; index < quantity; index++) {
    let key;
    do { key = generateKey(); } while (db.keys[key]);
    db.keys[key] = {
      duration_seconds: quote.durationSeconds, start_unix: null, expiry_unix: null, hwid: null,
      games: [...entitledGames],
      created_at: current, pause_reasons: [], pause_started_at: null,
      reseller_discord_id: discordId, reseller_order_id: orderId,
      reseller_unit_price_cents: quote.unitPriceCents
    };
    keys.push(key);
  }

  const before = account.balance_cents;
  account.balance_cents -= quote.totalCents;
  account.total_spent_cents += quote.totalCents;
  account.total_keys_generated += quantity;
  account.updated_at = current;
  db.reseller_ledger.push({
    id: crypto.randomUUID(), reseller_discord_id: discordId, type: 'key_order', order_id: orderId,
    delta_cents: -quote.totalCents, balance_before_cents: before, balance_after_cents: account.balance_cents,
    product_id: productId, duration_seconds: quote.durationSeconds, quantity, games: [...entitledGames],
    unit_price_cents: quote.unitPriceCents, total_cents: quote.totalCents,
    tier: quote.tierLabel, discount_percent: quote.discountPercent,
    actor_discord_id: discordId, reason: 'Reseller key order', created_at: current
  });
  return { account, games: entitledGames, keys, orderId, quote };
}

module.exports = {
  MAX_BALANCE_CENTS, MAX_ORDER_QUANTITY, PRODUCTS, TIERS,
  addReseller, adjustResellerBalance, centsFromAmount, formatMoney,
  generateResellerOrder, quoteOrder, resellerAccount
};
