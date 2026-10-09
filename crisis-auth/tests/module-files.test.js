const assert = require('assert/strict');
const fs = require('fs');
const os = require('os');
const path = require('path');
const test = require('node:test');

const { createModuleFileManager, normalizeModuleName, validatePortableExecutable } = require('../src/lib/module-files');

function portableExecutable(marker = 0) {
  const buffer = Buffer.alloc(128, marker);
  buffer[0] = 0x4d;
  buffer[1] = 0x5a;
  buffer.writeUInt32LE(64, 0x3c);
  buffer.writeUInt32LE(0x00004550, 64);
  return buffer;
}

test('module file manager validates names and PE content', () => {
  assert.equal(normalizeModuleName('memory'), 'crisis-memory.dll');
  assert.equal(normalizeModuleName('crisis-memory'), 'crisis-memory.dll');
  assert.equal(normalizeModuleName('crisis-memory.dll'), 'crisis-memory.dll');
  assert.equal(normalizeModuleName('overlay-test.dll'), 'crisis-overlay-test.dll');
  for (const name of ['../bad.dll', 'folder/bad.dll', 'bad name', '.dll', 'bad..dll']) assert.throws(() => normalizeModuleName(name));
  assert.equal(validatePortableExecutable(portableExecutable()), true);
  assert.equal(validatePortableExecutable(Buffer.from('MZ')), false);
});

test('module file manager creates, replaces, lists, and deletes with versions', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'crisis-modules-'));
  try {
    const manager = createModuleFileManager({ storageRoot: directory, maxUploadBytes: 1024 });
    const first = portableExecutable(1);
    const second = portableExecutable(2);
    assert.equal(manager.store('test', first, 'create').size, first.length);
    assert.throws(() => manager.store('crisis-test.dll', first, 'create'), /already exists/);
    assert.deepEqual(manager.list().map((file) => file.filename), ['crisis-test.dll']);
    assert.equal(manager.resolve('test').filename, 'crisis-test.dll');
    assert.equal(manager.resolve('crisis-test').filename, 'crisis-test.dll');
    assert.equal(manager.resolve('crisis-test.dll').filename, 'crisis-test.dll');
    assert.equal(manager.store('crisis-test', second, 'replace').size, second.length);
    assert.deepEqual(fs.readFileSync(path.join(directory, 'current', 'crisis-test.dll')), second);
    assert.equal(fs.readdirSync(path.join(directory, 'versions', 'modules')).length, 1);
    assert.equal(manager.remove('test').filename, 'crisis-test.dll');
    assert.deepEqual(manager.list(), []);
    assert.equal(fs.readdirSync(path.join(directory, 'versions', 'modules')).length, 2);
    assert.throws(() => manager.remove('crisis-test'), /does not exist/);
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});
