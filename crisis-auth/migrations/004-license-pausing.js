module.exports = {
  version: 4,
  description: 'Add composable global, administrator, and user subscription pause state.',
  up(db) {
    if (!db.service_state || typeof db.service_state !== 'object' || Array.isArray(db.service_state)) db.service_state = {};
    db.service_state.keys_paused = Boolean(db.service_state.keys_paused);
    db.service_state.keys_paused_at = Number.isSafeInteger(db.service_state.keys_paused_at) ? db.service_state.keys_paused_at : null;
    for (const license of Object.values(db.keys || {})) {
      if (!Array.isArray(license.pause_reasons)) license.pause_reasons = [];
      license.pause_reasons = [...new Set(license.pause_reasons.filter((value) => typeof value === 'string' && value.length <= 64))];
      if (!license.pause_reasons.length) license.pause_started_at = null;
    }
  }
};
