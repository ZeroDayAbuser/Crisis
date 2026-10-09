const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { createStore } = require('../src/lib/data-store');

test('Discord-generated licenses survive datastore and process recreation', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-persistence-'));
  const filename = path.join(directory, 'database.json');
  try {
    const generatedKey = 'PERSIST-RESTART-TEST-KEY';
    const firstProcess = createStore(filename);
    firstProcess.initialize();
    firstProcess.transaction((db) => {
      db.keys[generatedKey] = {
        duration_seconds: 86400, start_unix: null, expiry_unix: null, hwid: null,
        created_at: 1, pause_reasons: [], pause_started_at: null
      };
    });


    const restartedProcess = createStore(filename);
    const persisted = restartedProcess.read().keys[generatedKey];
    assert.ok(persisted);
    assert.equal(persisted.duration_seconds, 86400);

    restartedProcess.transaction((db) => {
      db.keys[generatedKey].start_unix = 100;
      db.keys[generatedKey].expiry_unix = 86500;
      db.keys[generatedKey].hwid = 'stable-volume-id';
    });

    const secondRestart = createStore(filename).read().keys[generatedKey];
    assert.equal(secondRestart.hwid, 'stable-volume-id');
    assert.equal(secondRestart.expiry_unix, 86500);
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

test('startup, migration checks, and no-op transactions do not rewrite an unchanged database', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-no-rewrite-'));
  const filename = path.join(directory, 'database.json');
  try {
    const store = createStore(filename);
    store.initialize();
    store.transaction((db) => {
      db.keys['UNCHANGED-KEY'] = {
        duration_seconds: 86400, start_unix: null, expiry_unix: null, hwid: null,
        created_at: 1, pause_reasons: [], pause_started_at: null
      };
    });

    const oldTime = new Date('2001-01-01T00:00:00.000Z');
    fs.utimesSync(filename, oldTime, oldTime);
    const before = fs.readFileSync(filename, 'utf8');
    const modifiedBefore = fs.statSync(filename).mtimeMs;

    createStore(filename).initialize();
    createStore(filename).transaction(() => {});

    assert.equal(fs.readFileSync(filename, 'utf8'), before);
    assert.equal(fs.statSync(filename).mtimeMs, modifiedBefore);
    assert.ok(createStore(filename).read().keys['UNCHANGED-KEY']);
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

test('a missing production datastore fails instead of restoring an empty example-shaped database', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-missing-production-db-'));
  const filename = path.join(directory, 'database.json');
  try {
    const store = createStore(filename, { allowCreate: false });
    assert.throws(() => store.initialize(), (error) => error?.code === 'ENOENT');
    assert.equal(fs.existsSync(filename), false);
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});
