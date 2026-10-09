const assert = require('assert/strict');
const { spawnSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');
const test = require('node:test');

const { loadConfiguration, parseEnv } = require('../src/lib/config');
const { emptyDatabase } = require('../src/lib/data-store');

const projectRoot = path.join(__dirname, '..');

function runNode(arguments_, environment) {
  return spawnSync(process.execPath, arguments_, {
    cwd: projectRoot,
    env: { ...process.env, ...environment },
    encoding: 'utf8'
  });
}

test('simple .env values load once and map friendly Discord names', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-config-'));
  const filename = path.join(directory, '.env');
  fs.writeFileSync(filename, [
    '# mock configuration',
    'DISCORD_BOT_TOKEN="mock.bot.token"',
    'DISCORD_CLIENT_ID=123456789012345678',
    'DISCORD_PUBLIC_KEY=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
    'DISCORD_GUILD_ID=234567890123456789',
    'DISCORD_ADMIN_ROLE_IDS=345678901234567890,456789012345678901',
    'DISCORD_ADMIN_USER_IDS=222222222222222222',
    'ADMIN_EMAIL=mock-admin@example.com',
    'ADMIN_PASSWORD="Mock password 42!"'
  ].join('\n'));
  const names = [
    'DISCORD_BOT_TOKEN', 'DISCORD_CLIENT_ID', 'DISCORD_PUBLIC_KEY', 'DISCORD_GUILD_ID', 'DISCORD_ADMIN_ROLE_IDS', 'DISCORD_ADMIN_USER_IDS',
    'CRISIS_DISCORD_BOT_TOKEN', 'CRISIS_DISCORD_APP_ID', 'CRISIS_DISCORD_PUBLIC_KEY', 'CRISIS_DISCORD_GUILD_ID', 'CRISIS_DISCORD_ADMIN_ROLE_IDS', 'CRISIS_DISCORD_ADMIN_USER_IDS',
    'ADMIN_EMAIL', 'ADMIN_PASSWORD'
  ];
  const previous = Object.fromEntries(names.map((name) => [name, process.env[name]]));
  for (const name of names) delete process.env[name];
  try {
    loadConfiguration(filename);
    assert.equal(process.env.CRISIS_DISCORD_BOT_TOKEN, 'mock.bot.token');
    assert.equal(process.env.CRISIS_DISCORD_APP_ID, '123456789012345678');
    assert.equal(process.env.CRISIS_DISCORD_ADMIN_ROLE_IDS, '345678901234567890,456789012345678901');
    assert.equal(process.env.CRISIS_DISCORD_ADMIN_USER_IDS, '222222222222222222');
    assert.equal(process.env.ADMIN_EMAIL, 'mock-admin@example.com');

    process.env.DISCORD_BOT_TOKEN = 'real-environment-wins';
    loadConfiguration(filename);
    assert.equal(process.env.DISCORD_BOT_TOKEN, 'real-environment-wins');
  } finally {
    for (const name of names) {
      if (previous[name] === undefined) delete process.env[name];
      else process.env[name] = previous[name];
    }
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

test('env parser supports comments and quoted mock values', () => {
  assert.deepEqual(parseEnv("ONE=value # comment\nTWO='two words'\nTHREE=\"line\\nvalue\"\n"), {
    ONE: 'value', TWO: 'two words', THREE: 'line\nvalue'
  });
});

test('manual server startup requires one absolute persistent database path', () => {
  const environment = { ...process.env };
  delete environment.DATABASE_PATH;
  const missing = spawnSync(process.execPath, ['server.js'], {
    cwd: projectRoot, env: environment, encoding: 'utf8'
  });
  assert.notEqual(missing.status, 0);
  assert.match(missing.stderr, /DATABASE_PATH is required/);

  const relative = runNode(['server.js'], { DATABASE_PATH: 'database.json' });
  assert.notEqual(relative.status, 0);
  assert.match(relative.stderr, /DATABASE_PATH must be absolute/);
});

test('configured database fails closed when missing and preserves keys across process restarts', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-persistence-'));
  const databasePath = path.join(directory, 'database.json');
  try {
    const absent = runNode(['server.js'], { DATABASE_PATH: databasePath });
    assert.notEqual(absent.status, 0);
    assert.equal(fs.existsSync(databasePath), false, 'server startup must not create a replacement database');

    const database = emptyDatabase();
    database.keys['PERSISTENT-KEY'] = {
      duration_seconds: 3600, games: ['fortnite'], start_unix: 1,
      expiry_unix: 4102444800, hwid: 'HWID', created_at: 1
    };
    fs.writeFileSync(databasePath, `${JSON.stringify(database, null, 2)}\n`);

    const verify = "const { loadDatabase } = require('./server'); if (!loadDatabase().keys['PERSISTENT-KEY']) process.exit(2);";
    assert.equal(runNode(['-e', verify], { DATABASE_PATH: databasePath }).status, 0);
    assert.equal(runNode(['-e', verify], { DATABASE_PATH: databasePath }).status, 0,
      'the same key must remain available after a fresh server process starts');
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});
