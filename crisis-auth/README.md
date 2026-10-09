# crisis-auth

`crisis-auth` is a CommonJS Node/Express service for license activation, HWID-bound legacy clients, player economy data, Discord member attestations, browser accounts, device authorization, and administration. Persistence remains a single JSON datastore to preserve the existing architecture. Writes are serialized within one Node process and committed by temporary-file rename.

## Repository layout

```text
crisis-auth/
├── src/
│   ├── app.js                 Express application and legacy routes
│   ├── native-server.js       Native TCP authentication listener
│   ├── account-service.js     Browser account, device, and admin APIs
│   ├── player-service.js      Player economy and game APIs
│   ├── discord/               Discord transport, verification, and routes
│   └── lib/                   Datastore, TCP framing, and shared security primitives
├── public/                    Browser HTML, CSS, and vanilla JavaScript
├── tests/
│   ├── *.test.js              Automated suites
│   ├── manual/                Command-line integration clients
│   └── fixtures/              Test artifacts
├── migrations/                Ordered datastore migrations
├── scripts/                   Migration and administrator utilities
├── client/                    Native C++ client sources
├── examples/                  Standalone native examples
├── drivers/ and modules/      Legacy downloadable artifacts
└── server.js                  Stable deployment compatibility entrypoint
```

Keeping the root `server.js` wrapper preserves the existing service installer and `node server.js` deployment command while application code remains under `src/`.

## Existing and new authentication models

The native loader submits its license key and HWID only over the TCP authentication protocol (`op=authenticate` on `CRISIS_NATIVE_PORT`, default 223); neither value is accepted in HTTP URLs, player routes, or the retired HTTP native endpoints. Every authentication request also carries a fresh timestamp, nonce, P-256 public key, and ECDSA signature. The first successful login binds that device key to the license; later logins must prove possession of its private key. The Windows client stores a persisted P-256 CNG key in the user software key store. HWID remains a secondary license association rather than proof of identity. License keys remain in the datastore because native authentication and the v2 Discord request MAC require the server to recover the original secret. Browser accounts never receive those values after redemption, and account-to-license links store only SHA-256 domain-separated digests.

Browser users authenticate with email and scrypt-hashed passwords. The server issues random, hashed-at-rest sessions in `HttpOnly`, `SameSite=Strict`, secure cookies. A separate same-site CSRF cookie supplies the synchronizer token required in `X-CSRF-Token`. Internal user IDs, hashes, sessions, license keys, raw HWIDs, and device credentials are omitted from browser responses.

Email delivery did not exist in the repository, so registration does not claim that mail was sent. The schema retains `email_verified`; enable verification only after integrating an approved transactional mail provider. Production administrators should be protected by identity-aware proxy MFA because this stack has no existing MFA provider.

## Local setup

1. Install Node.js 20 or newer and run `npm ci`.
2. Copy `.env.example` to `.env`, fill in the Discord and initial-admin fields, and set `DATABASE_PATH` to one absolute path on persistent storage. When recovering an existing installation, point it at the existing database before starting Node.
3. Back up the configured database, run `npm run migrate`, then run `npm run setup` once to create the initial web administrator. Use `database.example.json` only as a schema template.
4. Erase the `ADMIN_PASSWORD` value from `.env` after setup. Only its scrypt hash remains in the database.
5. Start with `npm start`. Browser, Discord, and player HTTP listen on `PORT` (default 1053). Native loader authentication, modules, and drivers use plaintext TCP on `CRISIS_NATIVE_PORT` (default 223). Production HTTP must sit behind HTTPS with `X-Forwarded-Proto` preserved.

The short configuration names are `DISCORD_BOT_TOKEN`, `DISCORD_CLIENT_ID`, `DISCORD_PUBLIC_KEY`, `DISCORD_GUILD_ID`, `DISCORD_ADMIN_ROLE_IDS`, `DISCORD_ADMIN_USER_IDS`, `ADMIN_EMAIL`, and `ADMIN_PASSWORD`. IDs and the public key are not secrets. The bot token and temporary setup password are secrets.

Do not run multiple service processes against one JSON file. Multi-instance deployment requires replacing the store with a transactional database and distributed rate limiting. Restrict the datastore, upload root, backups, and environment to the service account.

## Migrations and admin provisioning

Migrations are ordered files under `migrations/`. The service applies them on startup, and `npm run migrate` provides an explicit deployment step. Always back up before migrating.

No default password is created automatically. Put a unique initial email/password in `.env`, then provision it once:

```powershell
npm run setup
```

Erase `ADMIN_PASSWORD` from `.env` immediately afterward. Running setup again with a deliberate new password updates the administrator account.

Provisioning is a local privileged operation. There is intentionally no browser role-edit endpoint, preventing self-role changes and browser privilege escalation. The status endpoint rejects self-suspension and suspension of the final active administrator.

The compatibility `POST /admin/create-key` endpoint remains available to a narrowly scoped license-issuer service protected by `CRISIS_ADMIN_SECRET`. It is rate-limited and requires `{ "key": "...", "duration_seconds": 2592000, "games": ["fortnite"], "reason": "..." }`; valid game IDs are currently `fortnite` and `rust`. Creation is recorded using only a license digest in the audit target. It must not be exposed as an interactive administrator login.

## Device authorization flow

1. A device requests an authorization and receives a 256-bit device code, an eight-character user code, verification URL, ten-minute expiry, and five-second polling interval.
2. A signed-in user opens `/device`, enters the user code, reviews the device name/scopes, and approves or denies it.
3. The device polls no faster than the returned interval. Responses are `authorization_pending`, `slow_down`, `access_denied`, `expired_token`, or `authorized` and never contain account identity while pending.
4. The successful poll atomically consumes the device code and returns one scoped device credential. Only its digest is stored. Credentials can be revoked by the user or rotated by the device.

Sample requests (dummy values only):

```http
POST /api/device/authorization
Content-Type: application/json

{"device_name":"Living room PC","scopes":["account:read"]}
```

```http
POST /api/device/token
Content-Type: application/json

{"device_code":"dummy-device-code-returned-by-the-first-request"}
```

```http
GET /api/device/account
Authorization: Device dummy-one-time-device-credential
```

## Discord bot and license pairing

The Discord bot processes commands through signed interaction webhooks and opens a minimal Discord Gateway connection so its account visibly stays online. The Gateway requests no privileged intents and does not process messages. Set the Discord application's Interactions Endpoint URL to `https://wauth.aaddpp.lol/api/discord/interactions`. Customers only enter their license in the loader and use the pairing code shown in the Fortnite profile card; they do not configure URLs or environment variables. An unconsumed code remains stable across loader launches and expires with the license.

On a healthy startup, the log includes `Verified credentials, public key, and interaction endpoint...`, `Registered 23 global commands...`, and `Gateway online as ...`. The preflight obtains the current application from Discord and rejects a client ID, public key, or endpoint belonging to a different application. Incoming commands also log their command name and completion time without logging keys, tokens, or command options.

The server operator fills in the `DISCORD_*` fields in `.env`. `DISCORD_ADMIN_ROLE_IDS` authorizes administrators while commands are used inside the configured server. `DISCORD_ADMIN_USER_IDS` authorizes those Discord accounts everywhere, including bot DMs and user-installed contexts; configure it for every administrator who needs out-of-server access. When neither allowlist matches, guild commands require Discord's Administrator permission and DM admin commands fail closed. The bot token must never be shipped to customers.

In the Discord Developer Portal, open **Installation**, enable both **Guild Install** and **User Install**, and keep the Discord-provided install link. User installation needs the `applications.commands` scope. Commands are registered globally with `GUILD`, `BOT_DM`, and `PRIVATE_CHANNEL` contexts; the service removes obsolete guild-only command copies during startup.

User commands are `/link code:`, `/pair code:`, `/time_left`, `/request_hwid_reset`, `/pause_subscription`, `/unpause_subscription`, and `/reseller`. Both linking commands consume the 12-character code displayed by the native client; users never submit their license key to Discord. `/reseller` is available only to active reseller accounts and opens an ephemeral modal for balance details or key generation. Generated batches are returned as an ephemeral text attachment so large orders do not expose keys in ordinary channel messages.

Administrator commands are `/generate`, `/add_reseller`, `/reseller_balance`, `/reset_user`, `/reset_key`, `/pause_keys`, `/resume_keys`, `/pause_key`, `/resume_key`, `/pause_user`, `/resume_user`, `/admin_user`, `/admin_time`, `/admin_reset_requests`, and `/admin_reset_review`. `/add_reseller` creates an account with an optional starting USD balance. `/reseller_balance` atomically adds, sets, or removes an amount and requires an audit reason. Balances are stored as integer cents; overdrafts and amounts with more than two decimal places are rejected. Global, administrator, and user pauses compose safely: removing one pause never bypasses another, and elapsed subscription time is restored only after the final pause is removed. Responses are ephemeral, ordinary status responses mask keys, and every privileged mutation is audited.

Reseller orders use the quantity tier selected for that complete order:

| Quantity | Discount | 1 Day | 3 Day | 1 Week | 1 Month |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1–4 | 25% | $6.74 | $11.24 | $26.24 | $44.99 |
| 5–9 | 30% | $6.29 | $10.49 | $24.49 | $41.99 |
| 10–24 | 35% | $5.84 | $9.74 | $22.74 | $38.99 |
| 25–49 | 40% | $5.39 | $8.99 | $20.99 | $35.99 |
| 50–99 | 45% | $4.94 | $8.24 | $19.24 | $32.99 |
| 100+ | 50% | $4.50 | $7.50 | $17.50 | $30.00 |

Each order is one datastore transaction: the service verifies the balance, creates unique inactive licenses, debits the exact cent total, and writes reseller and audit ledger entries together. A failed or underfunded order creates no keys and changes no balance. One modal submission may generate up to 1,000 keys.

Production rejects requests that are not marked HTTPS by a trusted reverse proxy. The default trusts forwarded protocol headers only from loopback. Terminate TLS on the same host when possible; otherwise set `CRISIS_TRUST_PROXY` to the proxy's exact IP/CIDR, preserve `X-Forwarded-Proto`, and run with `NODE_ENV=production`.

The native loader authenticates with a length-prefixed plaintext TCP protocol (`CAUT` frames) on `CRISIS_NATIVE_PORT` using its device-key proof. A successful response includes the license's explicit `games` entitlement list and returns two opaque bearer credentials: a five-minute artifact token accepted only by the TCP `module` and `driver` operations, plus a player token accepted only by HTTP `/v1/player/*` and bounded by the subscription expiry. Artifact requests require both the bearer token and a fresh device signature bound to the operation, artifact name, token, timestamp, and nonce; captured proofs are single-use. Reauthentication rotates and revokes both previous tokens for that license. Process restarts intentionally invalidate all outstanding native tokens. HTTP native routes, query-string license/HWID authentication, and unsigned native authentication are rejected. An approved HWID/device reset clears both bindings so the next signed login can enroll replacement hardware.

Native clients cannot assert a Discord ID during authentication. The server-issued pairing code must be consumed through the Discord bot, and signed member-info responses are issued only for the Discord account already paired to that license.

## Server-authoritative profiles, XP, and leaderboard

`GET /v1/player` is the canonical bearer-authenticated profile snapshot consumed by `crisis-fortnite`. It includes server-stored identity and role data, subscription expiry, spendable and lifetime XP, Crisis Coins, server-derived level boundaries/progress, gameplay totals, K/D, win rate, recent XP earnings, lifetime progression points, and the nearby lifetime-XP leaderboard. The DLL no longer contains seeded profile statistics, fake leaderboard users, fabricated XP history, or a local level formula.

The Fortnite integration reports a match lifecycle under `/v1/player/gameplay/*` using unique match/request/event identifiers. The server owns reward amounts, counters, progression, placement, duplicate suppression, active-match state, per-match elimination limits, and minimum victory duration. Arbitrary client writes to `/stats` return `410 Gone`. XP-based license extensions are validated, deducted, extended, and ledgered atomically by `/v1/player/subscription/redeem-xp`.

Authenticated client telemetry is still a signal from a customer-controlled machine, not independent proof that an in-game event occurred. For competitive or monetary rewards, ingest events through the server-only `/xp/earn` integration using `CRISIS_EVENT_SECRET` or another trusted game-data provider.

## Authoritative and provably-fair casino

Mines, Roulette, Blackjack, Chicken, Crash, and feature crates use the authenticated routes under `/v1/player/casino/*`. The server owns every balance, wager, deck, mine position, jump result, crash point, crate reward, payout, and round state. `crisis-menu` and `crisis-fortnite` only submit player choices and animate the returned result; they never select an outcome or credit coins. The older `/gamble`, `/games/*`, and `/crates/open` routes return `410 Gone` so clients cannot bypass the fairness protocol.

Before a wager, `GET /v1/player/casino/state` returns the SHA-256 commitment for the next server seed and the fixed payout rules. A round derives its result with `HMAC-SHA256(server_seed, client_seed:nonce:game:cursor)`. Integer draws use rejection sampling instead of modulo-biased mapping. The seed remains hidden while an interactive round is active, is revealed on settlement, and is immediately replaced by a newly committed seed. Clients verify that `SHA-256(revealed_server_seed)` equals the earlier commitment.

Every request carries a unique `request_id`; the server stores and replays the first response so a network retry cannot debit or pay twice. Wagers and payouts are separate `coin_ledger` rows linked by round ID, and all mutations run inside the database transaction. The published rules use European single-zero Roulette (green pays 36x gross), standard dealer-stands-on-17 Blackjack with a 3:2 natural, a 1% Mines/Crash house factor, and fixed Chicken survival/payout tables. Bets are integer Crisis Coins from 1 through 1,000,000.

The production loader passes only its authenticated URL and scoped player token to the streamed `crisis-fortnite` child, then removes its environment copy immediately after process creation. The five-minute artifact token is erased after downloads, and the long-lived license key and HWID are never passed to child modules. Non-loopback traffic is refused unless it uses HTTPS.

## Administration and audit integrity

The `/admin` interface searches, filters, and paginates users; shows status, time, coins, devices, and audit history; and supports atomic coin/time adjustments, suspension/restoration, legacy HWID reset, and approved file replacement. Mutations require an administrator role, CSRF protection, rate controls where applicable, explicit confirmation in the UI, and a 3–500 character reason. Support-role accounts are read-only.

Coin and subscription changes append ledger rows containing previous/new values. Admin actions append audit events containing actor, target, action, reason, timestamp, request ID, and bounded IP/user-agent metadata. Audit entries never contain credentials or uploaded bytes.

## Upload storage and rollback

Only these slots are accepted: `driver` (`driver.sys`), `module-rust` (`rust.dll`), and `module-test` (`test-dll.dll`). The server rejects arbitrary paths and validates extension, allowed MIME, maximum size, DOS `MZ` bytes, PE offset, and PE signature. Files receive server-defined names under `CRISIS_UPLOAD_ROOT/current`, outside `public/`; existing versions are copied to `versions/<slot>` before an atomic rename. Uploads are never executed by the web process. Legacy arbitrary-name upload routes were removed because they could not meet these controls.

Discord administrators can use `/modules` to open the module manager modal. It supports listing, creating, replacing, and deleting DLL modules under `CRISIS_UPLOAD_ROOT/current`. Short names are normalized automatically: an upload named `memory` is stored as `crisis-memory.dll`, and clients can request it with the TCP `module` operation using `name=memory`. The forms `crisis-memory` and `crisis-memory.dll` resolve to the same artifact. Module listings show the access level: `crisis-fortnite.dll`, `fortnite.dll`, and their `-`-suffixed variants require Fortnite access; the equivalent Rust names require Rust access; other modules such as memory, overlay, and core are universal. Filenames are path-safe, uploads are size-limited and PE-validated, actions are audited, and replacements or deletions retain rollback copies under `versions/modules`.

Rollback is an operator action: stop the service, verify the selected version hash and permissions, copy it to a new temporary file under `current`, atomically rename it to the slot filename, record the operational reason, and restart. Automated unaudited rollback is intentionally unavailable.

## Security deployment requirements

- Terminate modern TLS before the service, set `NODE_ENV=production`, and preserve HSTS/CSP/security headers.
- Keep the default loopback-only proxy trust, or set `CRISIS_TRUST_PROXY` to the exact reverse-proxy IP/CIDR; never use a blanket hop count on an Internet-reachable listener.
- Set an explicit `CRISIS_CORS_ORIGINS`; wildcard origins are never used with credentials.
- Use independent high-entropy event and legacy admin secrets, rotate them, and restrict legacy admin access by network policy.
- Put MFA in front of `/admin`, limit it to an administrative network, and monitor authentication, rate-limit, and audit events.
- Never place passwords, cookies, license/device codes, credentials, Discord tokens, private keys, or upload contents in logs.
- Back up the JSON database and rollback directory. Append-only logical audit records do not prevent an operating-system administrator from editing the JSON file; use a managed append-only audit sink for stronger tamper resistance.
- Keep `DATABASE_PATH` on persistent storage outside the application checkout. The systemd installer uses `/var/lib/crisis-auth/database.json`, migrates an older live database only when the persistent file is absent, and explicitly creates the persistent file during first installation. `database.example.json` is never a runtime restore source. Manual and production startup require an absolute `DATABASE_PATH`; a missing path or missing live database stops startup instead of creating an empty replacement.
- The per-session XOR artifact mask is compatibility obfuscation, not cryptography or anti-dump protection. Native authentication uses plaintext TCP; browser and player HTTP still require HTTPS. Device proof prevents the reported key/HWID emulator and a stolen bearer token from downloading artifacts without the enrolled private key, but any module delivered to an authorized customer-controlled process can ultimately be extracted.

## Tests

Run `npm run check` and `npm test`. The security suite covers password sessions, normal-user admin denial, CSRF failure, device single-use/concurrent approval/expiry/replay, native device-key binding, stolen-token denial, artifact-proof replay denial, concurrent coin adjustments, time/HWID transactions, upload validation, login throttling, authoritative profile progression, forged-stat rejection, gameplay-event idempotency, XP redemption, fairness recomputation, hidden active-round state, idempotent casino retries, and authoritative payouts.
