# Crisis economy security

The menu is an untrusted client. Never place `CRISIS_EVENT_SECRET`,
`CRISIS_ADMIN_SECRET`, database credentials, RNG state, decks, or unpublished
odds in the executable.

## Automatic Linux installation

Place `install-service.sh` in the deployed `crisis-auth` folder (it is included
in this project), then run it as the normal Linux account that should own the
service:

```bash
chmod +x install-service.sh
./install-service.sh
```

The installer elevates only its setup operations with `sudo`. It generates and
stores both secrets in `/etc/crisis-auth.env`, installs production dependencies,
creates and hardens `crisis-auth.service`, enables it at boot, starts it, and
checks that it is active. Valid existing secrets are preserved on reruns.

Running it directly from a root shell is supported and will run the service as
root. To explicitly select root when launching from another account:

```bash
CRISIS_SERVICE_USER=root ./install-service.sh
```

Running the Node service as an unprivileged account remains safer when the
deployment location permits it.

## Required production settings

Set two independent, randomly generated secrets of at least 32 characters:

```powershell
$env:CRISIS_EVENT_SECRET = '<random server-only event secret>'
$env:CRISIS_ADMIN_SECRET = '<different random admin secret>'
```

- `CRISIS_EVENT_SECRET` authorizes XP awards from a trusted gameplay service.
  Send it only in the `X-Crisis-Event-Secret` header over HTTPS. It must never
  be sent to or stored by `crisis-menu`.
- `CRISIS_ADMIN_SECRET` protects every `/admin/*` route. Send it as an
  `Authorization: Bearer <secret>` header over HTTPS.
- Use different values for the two secrets and rotate them if either is exposed.

## Profile and gameplay event trust

Profile balances, levels, derived statistics, XP reward values, history, and leaderboard placement are calculated and persisted by the server. `/v1/player/stats` is retired so a native client cannot submit an arbitrary level, win rate, or K/D. Gameplay telemetry uses server-managed active matches, bounded eliminations, minimum victory duration, and persistent event identifiers to reject simple replays.

Bearer-authenticated telemetry still originates on a customer-controlled computer and is not cryptographic proof of a Fortnite event. The scoped player token is bounded by the subscription expiry and cannot download artifacts. Do not attach cash value or high-trust competitive rewards to these signals. The `CRISIS_EVENT_SECRET` endpoint is the trust boundary for events supplied by an independently trusted gameplay service and the secret must never be embedded in either native client.

Discord identity must come from the server-issued pairing code consumed by the Discord bot. Native authentication rejects client-supplied Discord profile fields, and member-info attestations are limited to the Discord account already paired to the license.

## Native device authentication

Native authentication is proof-of-possession, not HWID authentication. The Windows loader creates a persisted P-256 CNG key in the user software key store, and the first successful authentication binds its public key to the license. Authentication, module, and driver traffic uses length-prefixed plaintext TCP frames on `CRISIS_NATIVE_PORT` (default 223), not HTTP. Authentication and artifact requests require fresh ECDSA signatures; artifact signatures cover the operation, artifact name, bearer token, timestamp, and nonce. The server rejects stale or replayed proofs. Approved HWID resets also clear the device-key binding. The retired HTTP native routes return 410.

Deploy the updated server and native loader together. Existing licenses enroll a device key on their first signed login after deployment; unsigned clients fail closed. Monitor this enrollment window because a party that already has a license key and matching HWID could win the first-login race before the legitimate updated client enrolls. Reset any suspicious binding through the audited device-reset workflow.

Optional casino throttling settings:

```powershell
$env:CRISIS_CASINO_RATE_WINDOW_MS = '10000'
$env:CRISIS_CASINO_RATE_MAX = '30'
```

## Deployment requirements

Set `DATABASE_PATH` to an absolute persistent location outside the deployed application tree. `install-service.sh` defaults to `/var/lib/crisis-auth/database.json`, preserves an existing configured database when upgrading, and grants write access only to the service account. The installer explicitly creates the file on first installation. After that, production fails closed if the path or file is missing; it never restores `database.example.json` or creates an empty replacement. Back up this file before deployment.

- Put the Node HTTP service behind HTTPS; never expose its plain HTTP port publicly. Forwarded protocol headers are trusted only from loopback by default; set `CRISIS_TRUST_PROXY` only to the exact remote proxy IP/CIDR when necessary. Native authentication is plaintext TCP on `CRISIS_NATIVE_PORT`.
- Restrict the database file and process environment to the service account.
- Do not log license keys, HWIDs, authorization headers, or event secrets.
- Restrict `/admin/*` by firewall or private network in addition to its secret.
- Back up and monitor the database, and alert on repeated HTTP 401, 403, or 429
  responses.
- Keep outcomes and balance mutations on the server. Client-side obfuscation can
  slow inspection, but it cannot establish trust.
- Treat every artifact delivered to an authorized customer process as extractable. Device proof blocks key/HWID emulation and stolen bearer-token use without the enrolled private key; it does not prevent an authorized process from dumping bytes after delivery. The per-session XOR mask does not provide cryptographic protection.

Production casino seeds use Node's operating-system-backed cryptographic RNG.
Each seed is committed before use, outcomes are derived with HMAC-SHA256 and
unbiased rejection sampling, and the seed is revealed only after settlement.
The native clients verify the commitment before presenting a settled result.
Wager and payout mutations are atomic, ledgered, rate-limited, and idempotent.
The retired casino routes return HTTP 410 so they cannot bypass these rules.
