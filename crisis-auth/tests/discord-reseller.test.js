const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');

const { createStore } = require('../src/lib/data-store');
const { commandDefinitions, handleCommand, handleResellerModal, updateDeferredResponse } = require('../src/discord/bot');
const { quoteOrder } = require('../src/reseller-service');

const USER_ID = '111111111111111111';
const ADMIN_ID = '222222222222222222';
const OTHER_ID = '333333333333333333';

function option(name, value) { return { name, value }; }
function interaction(name, options = [], admin = false, userId = USER_ID) {
  return {
    data: { name, options },
    member: {
      permissions: admin ? '8' : '0', roles: [],
      user: { id: userId, username: `user-${userId.slice(-4)}`, global_name: 'Test User', avatar: null }
    }
  };
}
function resellerModalInteraction(action, product = null, quantity = '', games = ['fortnite'], userId = USER_ID) {
  const result = interaction('unused', [], false, userId);
  result.type = 5;
  result.data = {
    custom_id: 'reseller:manager:v1',
    components: [
      { type: 18, component: { type: 3, custom_id: 'reseller_action', values: [action] } },
      { type: 18, component: { type: 3, custom_id: 'reseller_product', values: product ? [product] : [] } },
      { type: 18, component: { type: 3, custom_id: 'reseller_games', values: games } },
      { type: 18, component: { type: 4, custom_id: 'reseller_quantity', value: quantity } }
    ]
  };
  return result;
}
function content(result) { return result.data.content; }

const EXPECTED_PRICES = [
  [1, 25, [674, 1124, 2624, 4499]],
  [5, 30, [629, 1049, 2449, 4199]],
  [10, 35, [584, 974, 2274, 3899]],
  [25, 40, [539, 899, 2099, 3599]],
  [50, 45, [494, 824, 1924, 3299]],
  [100, 50, [450, 750, 1750, 3000]]
];
const PRODUCTS = ['one_day', 'three_days', 'one_week', 'one_month'];

test('schema version 4 databases migrate reseller collections and legacy game access without losing licenses', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-reseller-migration-'));
  try {
    const filename = path.join(directory, 'database.json');
    fs.writeFileSync(filename, JSON.stringify({
      schema_version: 4,
      keys: { 'EXISTING-KEY': { duration_seconds: 86400 } },
      service_state: { keys_paused: false, keys_paused_at: null }
    }));
    const database = createStore(filename).initialize();
    assert.equal(database.schema_version, 6);
    assert.ok(database.keys['EXISTING-KEY']);
    assert.deepEqual(database.keys['EXISTING-KEY'].games, ['fortnite', 'rust']);
    assert.deepEqual(database.resellers, {});
    assert.deepEqual(database.reseller_ledger, []);
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

test('reseller quantity tiers use the configured cent-exact price table', () => {
  for (const [quantity, discount, prices] of EXPECTED_PRICES) {
    PRODUCTS.forEach((product, index) => {
      const quote = quoteOrder(product, quantity);
      assert.equal(quote.discountPercent, discount);
      assert.equal(quote.unitPriceCents, prices[index]);
      assert.equal(quote.totalCents, prices[index] * quantity);
    });
  }
  assert.equal(quoteOrder('one_day', 4).unitPriceCents, 674);
  assert.equal(quoteOrder('one_day', 9).unitPriceCents, 629);
  assert.equal(quoteOrder('one_day', 24).unitPriceCents, 584);
  assert.equal(quoteOrder('one_day', 49).unitPriceCents, 539);
  assert.equal(quoteOrder('one_day', 99).unitPriceCents, 494);
  assert.throws(() => quoteOrder('one_day', 0), /between 1 and/);
  assert.throws(() => quoteOrder('invalid', 1), /valid key duration/);
});

test('admins create reseller accounts and atomically add, set, or remove balances', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-reseller-admin-'));
  try {
    const store = createStore(path.join(directory, 'database.json'));
    store.initialize();
    const services = { store };

    const denied = handleCommand(interaction('add_reseller', [option('user', USER_ID), option('starting_balance', 100)]), services);
    assert.match(content(denied), /Administrator permission/);

    const created = handleCommand(interaction('add_reseller', [option('user', USER_ID), option('starting_balance', 100)], true, ADMIN_ID), services);
    assert.match(content(created), /\$100\.00/);
    assert.equal(store.read().resellers[USER_ID].balance_cents, 10000);

    const add = handleCommand(interaction('reseller_balance', [option('user', USER_ID), option('action', 'add'), option('amount', 25.55), option('reason', 'Card payment received')], true, ADMIN_ID), services);
    assert.match(content(add), /\$125\.55/);
    const remove = handleCommand(interaction('reseller_balance', [option('user', USER_ID), option('action', 'remove'), option('amount', 5.55), option('reason', 'Balance correction')], true, ADMIN_ID), services);
    assert.match(content(remove), /\$120\.00/);
    const set = handleCommand(interaction('reseller_balance', [option('user', USER_ID), option('action', 'set'), option('amount', 1000), option('reason', 'Confirmed account balance')], true, ADMIN_ID), services);
    assert.match(content(set), /\$1000\.00/);

    const before = store.read().resellers[USER_ID].balance_cents;
    const rejected = handleCommand(interaction('reseller_balance', [option('user', USER_ID), option('action', 'remove'), option('amount', 1000.01), option('reason', 'Invalid overdraft')], true, ADMIN_ID), services);
    assert.match(content(rejected), /outside the allowed range/);
    const database = store.read();
    assert.equal(database.resellers[USER_ID].balance_cents, before);
    assert.equal(database.reseller_ledger.length, 4);
    assert.ok(database.audit_events.some((entry) => entry.action === 'reseller.create'));
    assert.ok(database.audit_events.some((entry) => entry.action === 'reseller.balance.set'));
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

test('reseller modal reports balances and generates priced key batches as an attachment', async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-reseller-order-'));
  try {
    const store = createStore(path.join(directory, 'database.json'));
    store.initialize();
    const services = { store };
    handleCommand(interaction('add_reseller', [option('user', USER_ID), option('starting_balance', 5000)], true, ADMIN_ID), services);

    assert.match(content(handleCommand(interaction('reseller', [], false, OTHER_ID), services)), /do not have an active reseller/);
    const modal = handleCommand(interaction('reseller'), services);
    assert.equal(modal.type, 9);
    assert.equal(modal.data.custom_id, 'reseller:manager:v1');
    const gamesComponent = modal.data.components.find((component) => component.component.custom_id === 'reseller_games').component;
    assert.deepEqual(gamesComponent.options.map((entry) => entry.value), ['fortnite', 'rust']);
    assert.equal(gamesComponent.max_values, 2);

    const balance = await handleResellerModal(resellerModalInteraction('balance'), services);
    assert.match(content(balance), /\$5000\.00/);

    const generated = await handleResellerModal(resellerModalInteraction('generate', 'one_day', '100', ['fortnite', 'rust']), services);
    assert.ok(generated.file);
    assert.match(content(generated), /100 × 1 Day/);
    assert.match(content(generated), /\$4\.50 each/);
    assert.match(content(generated), /\$450\.00 total/);
    assert.match(content(generated), /Fortnite \+ Rust/);
    const fileLines = generated.file.buffer.toString('utf8').split('\n');
    const keys = fileLines.filter((line) => /^CRISIS-[A-Z2-9-]+$/.test(line));
    assert.equal(keys.length, 100);
    assert.equal(new Set(keys).size, 100);

    const database = store.read();
    assert.equal(Object.keys(database.keys).length, 100);
    assert.equal(database.resellers[USER_ID].balance_cents, 455000);
    assert.equal(database.resellers[USER_ID].total_spent_cents, 45000);
    assert.equal(database.resellers[USER_ID].total_keys_generated, 100);
    assert.ok(keys.every((key) => database.keys[key].duration_seconds === 86400));
    assert.ok(keys.every((key) => JSON.stringify(database.keys[key].games) === JSON.stringify(['fortnite', 'rust'])));
    assert.ok(keys.every((key) => database.keys[key].reseller_discord_id === USER_ID));
    assert.ok(database.reseller_ledger.some((entry) => entry.type === 'key_order' && entry.quantity === 100));
    assert.ok(database.reseller_ledger.some((entry) => entry.type === 'key_order' && JSON.stringify(entry.games) === JSON.stringify(['fortnite', 'rust'])));
    assert.ok(database.audit_events.some((entry) => entry.action === 'reseller.keys.generate'));

    handleCommand(interaction('reseller_balance', [option('user', USER_ID), option('action', 'set'), option('amount', 0), option('reason', 'Testing insufficient funds')], true, ADMIN_ID), services);
    const keyCount = Object.keys(store.read().keys).length;
    const rejected = await handleResellerModal(resellerModalInteraction('generate', 'one_month', '1'), services);
    assert.match(content(rejected), /Insufficient reseller balance/);
    assert.equal(Object.keys(store.read().keys).length, keyCount);
    assert.equal(rejected.file, undefined);
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

test('Discord command registration includes reseller entry points', () => {
  const commands = new Map(commandDefinitions().map((command) => [command.name, command]));
  for (const name of ['add_reseller', 'reseller_balance', 'reseller']) assert.ok(commands.has(name), name);
  assert.deepEqual(commands.get('reseller_balance').options.find((entry) => entry.name === 'action').choices.map((entry) => entry.value), ['add', 'set', 'remove']);
});

test('deferred reseller responses upload generated keys as an ephemeral attachment', async () => {
  let captured;
  const fetchImpl = async (url, request) => {
    captured = { url, request };
    return { ok: true, status: 200 };
  };
  const payload = {
    type: 4,
    data: { content: 'Keys attached', flags: 64, allowed_mentions: { parse: [] } },
    file: { filename: 'keys.txt', buffer: Buffer.from('KEY-1\nKEY-2', 'utf8') }
  };
  await updateDeferredResponse({ application_id: '123456789012345678', token: 'interaction-token' }, payload, fetchImpl);
  assert.equal(captured.url, 'https://discord.com/api/v10/webhooks/123456789012345678/interaction-token/messages/@original');
  assert.equal(captured.request.method, 'PATCH');
  assert.ok(captured.request.body instanceof FormData);
  const body = JSON.parse(captured.request.body.get('payload_json'));
  assert.deepEqual(body.attachments, [{ id: 0, filename: 'keys.txt' }]);
  assert.equal(await captured.request.body.get('files[0]').text(), 'KEY-1\nKEY-2');
});
