module.exports = {
  version: 5,
  description: 'Add reseller accounts, balances, and immutable balance ledger entries.',
  up(db) {
    if (!db.resellers || typeof db.resellers !== 'object' || Array.isArray(db.resellers)) db.resellers = {};
    for (const [discordId, account] of Object.entries(db.resellers)) {
      if (!/^\d{17,20}$/.test(discordId) || !account || typeof account !== 'object' || Array.isArray(account)) {
        delete db.resellers[discordId];
        continue;
      }
      account.discord_id = discordId;
      account.status = account.status === 'disabled' ? 'disabled' : 'active';
      account.balance_cents = Number.isSafeInteger(account.balance_cents) && account.balance_cents >= 0 ? account.balance_cents : 0;
      account.total_spent_cents = Number.isSafeInteger(account.total_spent_cents) && account.total_spent_cents >= 0 ? account.total_spent_cents : 0;
      account.total_keys_generated = Number.isSafeInteger(account.total_keys_generated) && account.total_keys_generated >= 0 ? account.total_keys_generated : 0;
      account.created_at = Number.isSafeInteger(account.created_at) ? account.created_at : Math.floor(Date.now() / 1000);
      account.updated_at = Number.isSafeInteger(account.updated_at) ? account.updated_at : account.created_at;
      account.created_by = typeof account.created_by === 'string' ? account.created_by : null;
    }
    if (!Array.isArray(db.reseller_ledger)) db.reseller_ledger = [];
  }
};
