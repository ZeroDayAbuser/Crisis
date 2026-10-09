const express = require('express');
const { decryptRequestEnvelope, encryptResponseEnvelope, loadTransportPrivateKey, parseRequestEnvelope } = require('./envelope');
const { authenticateDiscordRequest, validateRequestShape } = require('./request-auth');
const { DiscordApiError, DiscordMemberService } = require('./member-service');
const { signDiscordAttestation } = require('./response-signing');

const ROUTE_PATH = '/v2/discord/member-info';
const RATE_WINDOW_MS = 60 * 1000;
const RATE_CACHE_MAX_ENTRIES = 4096;
const NONCE_CACHE_MAX_ENTRIES = 10000;
const NONCE_TTL_MS = 120 * 1000;

function positiveInteger(value, fallback) {
  const parsed = Number(value);
  return Number.isSafeInteger(parsed) && parsed > 0 ? parsed : fallback;
}

function discordConfiguration(env = process.env) {
  return {
    botToken: env.DISCORD_BOT_TOKEN,
    guildId: env.DISCORD_GUILD_ID,
    apiTimeoutMs: positiveInteger(env.DISCORD_API_TIMEOUT_MS, 5000),
    roleCacheMs: positiveInteger(env.DISCORD_ROLE_CACHE_MS, 30000),
    signingPrivateKeyFile: env.DISCORD_SIGNING_PRIVATE_KEY_FILE,
    transportPrivateKeyFile: env.DISCORD_TRANSPORT_PRIVATE_KEY_FILE,
    signingKeyId: env.DISCORD_SIGNING_KEY_ID,
    signingIssuer: env.DISCORD_SIGNING_ISSUER || 'crisis-auth-discord-member-info',
    signingAudience: env.DISCORD_SIGNING_AUDIENCE || 'windows-client'
  };
}

function configurationIsComplete(config) {
  return typeof config.botToken === 'string' && config.botToken.length > 0 &&
    typeof config.guildId === 'string' && /^[0-9]{17,20}$/.test(config.guildId) &&
    typeof config.signingPrivateKeyFile === 'string' && config.signingPrivateKeyFile.length > 0 &&
    typeof config.transportPrivateKeyFile === 'string' && config.transportPrivateKeyFile.length > 0 &&
    typeof config.signingKeyId === 'string' && config.signingKeyId.length > 0;
}

function createRateLimiter(options = {}) {
  const now = options.now || (() => Date.now());
  const maxEntries = options.maxEntries || RATE_CACHE_MAX_ENTRIES;
  const ipLimit = options.ipLimit || 20;
  const licenseLimit = options.licenseLimit || 10;
  const ipEntries = new Map();
  const licenseEntries = new Map();

  function clean(entries, current) {
    for (const [key, entry] of entries) {
      if (entry.resetAt <= current) entries.delete(key);
    }
  }

  function take(entries, key, limit, current) {
    let entry = entries.get(key);
    if (!entry) {
      if (entries.size >= maxEntries) return { allowed: false, retryAfter: Math.ceil(RATE_WINDOW_MS / 1000) };
      entry = { count: 0, resetAt: current + RATE_WINDOW_MS };
      entries.set(key, entry);
    }
    if (entry.count >= limit) return { allowed: false, retryAfter: Math.max(1, Math.ceil((entry.resetAt - current) / 1000)) };
    entry.count += 1;
    return { allowed: true };
  }

  return {
    take(ip, licenseHash) {
      const current = now();
      clean(ipEntries, current);
      clean(licenseEntries, current);
      const byIp = take(ipEntries, ip, ipLimit, current);
      if (!byIp.allowed) return byIp;
      return take(licenseEntries, licenseHash, licenseLimit, current);
    }
  };
}

function createReplayCache(options = {}) {
  const now = options.now || (() => Date.now());
  const maxEntries = options.maxEntries || NONCE_CACHE_MAX_ENTRIES;
  const entries = new Map();

  function clean(current) {
    for (const [key, expiresAt] of entries) {
      if (expiresAt <= current) entries.delete(key);
    }
  }

  return {
    consume(licenseHash, nonce) {
      const current = now();
      clean(current);
      const key = `${licenseHash}:${nonce}`;
      if (entries.has(key)) return 'replayed';
      if (entries.size >= maxEntries) return 'full';
      entries.set(key, current + NONCE_TTL_MS);
      return 'accepted';
    }
  };
}

function createDiscordMemberRouter(options) {
  const router = express.Router();
  const now = options.now || (() => Date.now());
  const getConfig = options.getConfig || (() => discordConfiguration());
  const discordService = options.discordService || new DiscordMemberService({ now });
  const signAttestation = options.signAttestation || signDiscordAttestation;
  const rateLimiter = options.rateLimiter || createRateLimiter({ now });
  const replayCache = options.replayCache || createReplayCache({ now });

  router.post('/', express.json({ limit: '8kb', strict: true }), async (req, res) => {
    const envelope = parseRequestEnvelope(req.body);
    if (!envelope) return res.status(400).json({ success: false, error: 'Malformed request' });

    const config = getConfig();
    if (!configurationIsComplete(config)) {
      return res.status(503).json({ success: false, error: 'Discord service unavailable' });
    }

    let decrypted;
    try {
      decrypted = decryptRequestEnvelope(envelope, loadTransportPrivateKey(config.transportPrivateKeyFile));
    } catch {
      return res.status(503).json({ success: false, error: 'Discord service unavailable' });
    }
    if (!decrypted) return res.status(401).json({ success: false, error: 'Unauthorized' });

    const request = validateRequestShape(decrypted.body);
    if (!request) return res.status(400).json({ success: false, error: 'Malformed request' });

    const rate = rateLimiter.take(req.ip || req.socket.remoteAddress || 'unknown', request.license_hash);
    if (!rate.allowed) {
      res.setHeader('Retry-After', rate.retryAfter);
      return res.status(429).json({ success: false, error: 'Too many requests' });
    }

    const authenticated = authenticateDiscordRequest(request, options.loadDatabase(), Math.floor(now() / 1000));
    if (!authenticated) return res.status(401).json({ success: false, error: 'Unauthorized' });

    const replay = replayCache.consume(request.license_hash, request.nonce);
    if (replay === 'replayed') return res.status(401).json({ success: false, error: 'Unauthorized' });
    if (replay === 'full') return res.status(503).json({ success: false, error: 'Discord service unavailable' });

    try {
      const profile = await discordService.getMemberProfile(config, request.discord_id);
      const signingNow = Math.floor(now() / 1000);
      const attestation = await signAttestation({
        member: profile.member,
        guild_id: config.guildId,
        discord_id: request.discord_id,
        discord_id_hash: request.discord_id_hash,
        license_hash: request.license_hash,
        hwid_hash: request.hwid_hash,
        request_hash: authenticated.requestHash,
        nonce: request.nonce,
        username: profile.username,
        global_display_name: profile.global_display_name,
        display_name: profile.display_name,
        pfp_url: profile.pfp_url,
        roles: profile.roles,
        highest_role: profile.highest_role,
        observed_unix: profile.observed_unix
      }, config, signingNow);
      return res.json(encryptResponseEnvelope(attestation, decrypted.sharedSecret, request.nonce));
    } catch (error) {
      if (error instanceof DiscordApiError && error.kind === 'rate_limited') {
        res.setHeader('Retry-After', error.retryAfterSeconds || 1);
        return res.status(429).json({ success: false, error: 'Discord service unavailable' });
      }
      return res.status(503).json({ success: false, error: 'Discord service unavailable' });
    }
  });


  router.use((error, req, res, next) => {
    if (error && (error.type === 'entity.parse.failed' || error.type === 'entity.too.large')) {
      return res.status(400).json({ success: false, error: 'Malformed request' });
    }
    return next(error);
  });

  return router;
}

function mountDiscordMemberRoute(app, options) {
  app.use(ROUTE_PATH, createDiscordMemberRouter(options));
}

module.exports = {
  ROUTE_PATH,
  configurationIsComplete,
  createDiscordMemberRouter,
  createRateLimiter,
  createReplayCache,
  discordConfiguration,
  mountDiscordMemberRoute
};
