const { createStore } = require('../src/lib/data-store');
const { hashPassword, normalizeEmail, randomToken, validatePassword } = require('../src/lib/security');
const crypto = require('crypto');
const path = require('path');
require('../src/lib/config').loadConfiguration();

async function main() {
  const email = normalizeEmail(process.env.ADMIN_EMAIL || process.argv[2]);
  const password = process.env.ADMIN_PASSWORD || process.env.CRISIS_PROVISION_PASSWORD;
  if (!email || !validatePassword(password) || /^CHANGE_ME/i.test(password)) {
    throw new Error('Set ADMIN_EMAIL and a unique ADMIN_PASSWORD in .env, then run: npm run setup');
  }
  const store = createStore(process.env.DATABASE_PATH ? path.resolve(process.env.DATABASE_PATH) : path.join(__dirname, '..', 'database.json'));
  const passwordHash = await hashPassword(password); let outcome = 'created';
  store.transaction((db) => {
    const existing = Object.values(db.users).find((user) => user.email === email);
    if (existing) { existing.role = 'admin'; existing.status = 'active'; existing.password_hash = passwordHash; existing.updated_at = Math.floor(Date.now() / 1000); outcome = 'updated'; return; }
    const id = crypto.randomUUID(); const now = Math.floor(Date.now() / 1000);
    db.users[id] = { id, admin_ref: randomToken(18), email, password_hash: passwordHash, role: 'admin', status: 'active', email_verified: false, coin_balance: 0, subscription_expiry: null, failed_logins: 0, locked_until: 0, created_at: now, updated_at: now };
  });
  console.log(`Administrator ${outcome}: ${email}. Clear ADMIN_PASSWORD from .env now; the database retains only its password hash.`);
}
main().catch((error) => { console.error(error.message); process.exitCode = 1; });
