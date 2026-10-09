const https = require('https');

const MAX_DISCORD_RESPONSE_BYTES = 256 * 1024;

class DiscordApiError extends Error {
  constructor(kind, retryAfterSeconds) {
    super(kind);
    this.kind = kind;
    this.retryAfterSeconds = retryAfterSeconds;
  }
}

function retryAfterSeconds(headers) {
  const raw = headers['retry-after'] || headers['x-ratelimit-reset-after'];
  const parsed = Number(raw);
  return Number.isFinite(parsed) && parsed > 0 ? Math.ceil(parsed) : 1;
}

function discordGet(path, config) {
  return new Promise((resolve, reject) => {
    const request = https.request({
      protocol: 'https:',
      hostname: 'discord.com',
      port: 443,
      method: 'GET',
      path,
      headers: {
        Authorization: `Bot ${config.botToken}`,
        Accept: 'application/json',
        'User-Agent': 'crisis-auth-discord-member-info/2'
      }
    }, (response) => {
      const chunks = [];
      let length = 0;
      response.on('data', (chunk) => {
        length += chunk.length;
        if (length > MAX_DISCORD_RESPONSE_BYTES) {
          request.destroy(new Error('Discord response exceeded maximum size'));
          return;
        }
        chunks.push(chunk);
      });
      response.on('end', () => {
        const text = Buffer.concat(chunks).toString('utf8');
        let body = null;
        if (text) {
          try {
            body = JSON.parse(text);
          } catch {
            return reject(new DiscordApiError('malformed_response'));
          }
        }
        resolve({ status: response.statusCode, headers: response.headers, body });
      });
    });

    request.setTimeout(config.apiTimeoutMs, () => request.destroy(new Error('Discord request timed out')));
    request.on('error', (error) => {
      reject(new DiscordApiError(error.message === 'Discord request timed out' ? 'timeout' : 'network'));
    });
    request.end();
  });
}

function isSnowflake(value) {
  return typeof value === 'string' && /^[0-9]{17,20}$/.test(value);
}

function isNullableString(value) {
  return value === null || value === undefined || typeof value === 'string';
}

function defaultAvatarUrl(discordId) {
  const index = (BigInt(discordId) >> 22n) % 6n;
  return `https://cdn.discordapp.com/embed/avatars/${index}.png?size=256`;
}

function profilePictureUrl(guildId, user, member) {
  if (typeof member.avatar === 'string' && member.avatar) {
    return `https://cdn.discordapp.com/guilds/${guildId}/users/${user.id}/avatars/${member.avatar}.webp?size=256`;
  }
  if (typeof user.avatar === 'string' && user.avatar) {
    return `https://cdn.discordapp.com/avatars/${user.id}/${user.avatar}.webp?size=256`;
  }
  return defaultAvatarUrl(user.id);
}

function validateRole(role) {
  return role && typeof role === 'object' && !Array.isArray(role) &&
    isSnowflake(role.id) && typeof role.name === 'string' &&
    Number.isSafeInteger(role.position) && Number.isSafeInteger(role.color) &&
    typeof role.hoist === 'boolean' && typeof role.managed === 'boolean' &&
    typeof role.mentionable === 'boolean';
}

function roleIdDescending(left, right) {
  const leftId = BigInt(left.id);
  const rightId = BigInt(right.id);
  if (leftId === rightId) return 0;
  return leftId > rightId ? -1 : 1;
}

function toPublicRole(role) {
  return {
    id: role.id,
    name: role.name,
    position: role.position,
    color: role.color,
    hoist: role.hoist,
    managed: role.managed,
    mentionable: role.mentionable
  };
}

class DiscordMemberService {
  constructor(options = {}) {
    this.fetchDiscord = options.fetchDiscord || discordGet;
    this.now = options.now || (() => Date.now());
    this.roleCache = null;
  }

  async getRoles(config) {
    const now = this.now();
    if (this.roleCache && this.roleCache.guildId === config.guildId && this.roleCache.expiresAt > now) {
      return this.roleCache.roles;
    }

    const response = await this.fetchDiscord(`/api/v10/guilds/${config.guildId}/roles`, config);
    if (response.status === 429) throw new DiscordApiError('rate_limited', retryAfterSeconds(response.headers || {}));
    if (response.status < 200 || response.status >= 300 || !Array.isArray(response.body) || !response.body.every(validateRole)) {
      throw new DiscordApiError('roles_unavailable');
    }

    const roles = response.body.map(toPublicRole);
    this.roleCache = {
      guildId: config.guildId,
      roles,
      expiresAt: now + config.roleCacheMs
    };
    return roles;
  }

  async getMemberProfile(config, discordId) {
    const response = await this.fetchDiscord(`/api/v10/guilds/${config.guildId}/members/${discordId}`, config);
    if (response.status === 429) throw new DiscordApiError('rate_limited', retryAfterSeconds(response.headers || {}));
    if (response.status === 404 && response.body && response.body.code === 10007) {
      return {
        member: false,
        discord_id: discordId,
        pfp_url: null,
        username: null,
        display_name: null,
        global_display_name: null,
        roles: [],
        highest_role: null,
        observed_unix: Math.floor(this.now() / 1000)
      };
    }
    if (response.status < 200 || response.status >= 300) throw new DiscordApiError('member_unavailable');

    const member = response.body;
    if (!member || typeof member !== 'object' || Array.isArray(member) || !member.user || typeof member.user !== 'object' ||
      member.user.id !== discordId || typeof member.user.username !== 'string' || !member.user.username ||
      !Array.isArray(member.roles) || !member.roles.every(isSnowflake) ||
      !isNullableString(member.nick) || !isNullableString(member.user.global_name) || !isNullableString(member.user.avatar) ||
      !isNullableString(member.avatar)) {
      throw new DiscordApiError('malformed_member');
    }

    const guildRoles = await this.getRoles(config);
    const assignedIds = new Set(member.roles);
    const roles = guildRoles
      .filter((role) => role.id !== config.guildId && assignedIds.has(role.id))
      .sort((left, right) => right.position - left.position || roleIdDescending(left, right));
    const username = member.user.username;
    const globalDisplayName = member.user.global_name ?? username;
    const displayName = member.nick ?? globalDisplayName ?? username;

    return {
      member: true,
      discord_id: discordId,
      pfp_url: profilePictureUrl(config.guildId, member.user, member),
      username,
      display_name: displayName,
      global_display_name: globalDisplayName,
      roles,
      highest_role: roles[0] || null,
      observed_unix: Math.floor(this.now() / 1000)
    };
  }
}

module.exports = {
  DiscordApiError,
  DiscordMemberService,
  defaultAvatarUrl,
  discordGet,
  profilePictureUrl,
  roleIdDescending
};
