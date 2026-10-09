const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const MODULE_STEM = /^[A-Za-z0-9][A-Za-z0-9._-]{0,115}$/i;
const MODULE_NAME = /^[A-Za-z0-9][A-Za-z0-9._-]{0,122}\.dll$/i;

class ModuleFileError extends Error {
  constructor(message, code = 'invalid') {
    super(message);
    this.code = code;
  }
}

function normalizeModuleName(value) {
  const input = String(value || '').trim();
  if (!input || input.includes('..') || path.basename(input) !== input || /[\\/]/.test(input)) {
    throw new ModuleFileError('Use a module name containing only letters, numbers, dots, dashes, or underscores.');
  }
  const withoutExtension = input.replace(/\.dll$/i, '');
  if (!MODULE_STEM.test(withoutExtension)) throw new ModuleFileError('Use a module name containing only letters, numbers, dots, dashes, or underscores.');
  const stem = /^crisis-/i.test(withoutExtension) ? `crisis-${withoutExtension.slice(7)}` : `crisis-${withoutExtension}`;
  const name = `${stem}.dll`;
  if (!MODULE_NAME.test(name)) throw new ModuleFileError('The resulting DLL filename is too long.');
  return name;
}

function validatePortableExecutable(buffer) {
  if (!Buffer.isBuffer(buffer) || buffer.length < 64 || buffer[0] !== 0x4d || buffer[1] !== 0x5a) return false;
  const peOffset = buffer.readUInt32LE(0x3c);
  return peOffset >= 64 && peOffset + 4 <= buffer.length && buffer.readUInt32LE(peOffset) === 0x00004550;
}

function metadata(filename, buffer, modifiedAt = Date.now()) {
  return {
    filename,
    size: buffer.length,
    sha256: crypto.createHash('sha256').update(buffer).digest('hex'),
    modified_at: Math.floor(modifiedAt / 1000)
  };
}

function createModuleFileManager(options = {}) {
  const storageRoot = path.resolve(options.storageRoot || path.join(__dirname, '..', '..', 'storage'));
  const currentDirectory = path.join(storageRoot, 'current');
  const versionDirectory = path.join(storageRoot, 'versions', 'modules');
  const maxUploadBytes = Math.min(100 * 1024 * 1024, Math.max(1024, Number(options.maxUploadBytes) || 25 * 1024 * 1024));

  function destinationFor(filename) {
    return path.join(currentDirectory, normalizeModuleName(filename));
  }

  function list() {
    if (!fs.existsSync(currentDirectory)) return [];
    return fs.readdirSync(currentDirectory, { withFileTypes: true })
      .filter((entry) => entry.isFile() && /^crisis-/i.test(entry.name) && MODULE_NAME.test(entry.name) && !entry.name.includes('..'))
      .map((entry) => {
        const filename = normalizeModuleName(entry.name);
        const file = path.join(currentDirectory, filename);
        const stat = fs.statSync(file);
        return { filename, size: stat.size, modified_at: Math.floor(stat.mtimeMs / 1000) };
      })
      .sort((left, right) => left.filename.localeCompare(right.filename));
  }

  function resolve(name, fallbackDirectory = null) {
    const requested = String(name || '').trim();
    const exact = MODULE_NAME.test(requested) && !requested.includes('..') && path.basename(requested) === requested ? requested : null;
    let normalized = null;
    try { normalized = normalizeModuleName(requested); } catch {}
    const candidates = [
      exact && path.join(currentDirectory, exact),
      normalized && normalized !== exact && path.join(currentDirectory, normalized),
      exact && fallbackDirectory && path.join(path.resolve(fallbackDirectory), exact),
      normalized && normalized !== exact && fallbackDirectory && path.join(path.resolve(fallbackDirectory), normalized)
    ].filter(Boolean);
    const found = candidates.find((candidate) => fs.existsSync(candidate) && fs.statSync(candidate).isFile());
    return found ? { filename: path.basename(found), path: found } : null;
  }

  function store(filename, buffer, mode) {
    const normalized = normalizeModuleName(filename);
    if (!Buffer.isBuffer(buffer) || !buffer.length || buffer.length > maxUploadBytes) {
      throw new ModuleFileError(`The module must be between 1 byte and ${maxUploadBytes} bytes.`);
    }
    if (!validatePortableExecutable(buffer)) throw new ModuleFileError('The uploaded file is not a valid PE module.');
    if (!['create', 'replace'].includes(mode)) throw new ModuleFileError('Unsupported module operation.');

    fs.mkdirSync(currentDirectory, { recursive: true, mode: 0o700 });
    fs.mkdirSync(versionDirectory, { recursive: true, mode: 0o700 });
    const destination = destinationFor(normalized);
    const existed = fs.existsSync(destination);
    if (mode === 'create' && existed) throw new ModuleFileError(`${normalized} already exists. Use Replace / Update File.`, 'exists');
    if (mode === 'replace' && !existed) throw new ModuleFileError(`${normalized} does not exist. Use Create File.`, 'missing');

    const temporary = path.join(currentDirectory, `.${crypto.randomUUID()}.upload`);
    let backup = null;
    try {
      if (existed) {
        backup = path.join(versionDirectory, `${Date.now()}-${crypto.randomUUID()}-${normalized}`);
        fs.copyFileSync(destination, backup, fs.constants.COPYFILE_EXCL);
      }
      fs.writeFileSync(temporary, buffer, { mode: 0o600, flag: 'wx' });
      if (mode === 'create') {
        fs.copyFileSync(temporary, destination, fs.constants.COPYFILE_EXCL);
        fs.unlinkSync(temporary);
      } else {
        fs.renameSync(temporary, destination);
      }
      return metadata(normalized, buffer);
    } catch (error) {
      try { fs.unlinkSync(temporary); } catch {}
      if (backup) {
        try { fs.copyFileSync(backup, destination); } catch {}
      }
      if (error instanceof ModuleFileError) throw error;
      if (error.code === 'EEXIST') throw new ModuleFileError(`${normalized} already exists. Use Replace / Update File.`, 'exists');
      throw new ModuleFileError('The module could not be stored.', 'storage');
    }
  }

  function remove(filename) {
    const normalized = normalizeModuleName(filename);
    const destination = destinationFor(normalized);
    if (!fs.existsSync(destination)) throw new ModuleFileError(`${normalized} does not exist.`, 'missing');
    fs.mkdirSync(versionDirectory, { recursive: true, mode: 0o700 });
    const buffer = fs.readFileSync(destination);
    const previous = metadata(normalized, buffer, fs.statSync(destination).mtimeMs);
    const backup = path.join(versionDirectory, `${Date.now()}-${crypto.randomUUID()}-${normalized}`);
    try {
      fs.copyFileSync(destination, backup, fs.constants.COPYFILE_EXCL);
      fs.unlinkSync(destination);
      return previous;
    } catch {
      throw new ModuleFileError('The module could not be deleted.', 'storage');
    }
  }

  return { currentDirectory, list, maxUploadBytes, remove, resolve, store };
}

module.exports = { ModuleFileError, createModuleFileManager, normalizeModuleName, validatePortableExecutable };
