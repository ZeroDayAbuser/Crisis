const assert = require('assert/strict');
const test = require('node:test');
const { isLicensePaused, pauseAll, pauseLicense, remainingSeconds, resumeAll, resumeLicense } = require('../src/lib/license-state');

test('overlapping pause reasons freeze time until the final reason is removed', () => {
  const license = { start_unix: 100, expiry_unix: 1100, pause_reasons: [], pause_started_at: null };
  const db = { keys: { TEST: license }, service_state: { keys_paused: false, keys_paused_at: null } };

  assert.equal(pauseLicense(license, 'user-self', 200), true);
  assert.equal(remainingSeconds(license, 350), 900);
  assert.equal(pauseAll(db, 300), 1);
  assert.equal(isLicensePaused(db, license), true);

  assert.equal(resumeLicense(license, 'user-self', 400), true);
  assert.equal(license.expiry_unix, 1100, 'time is not restored while the global reason remains');
  assert.equal(resumeAll(db, 500), 1);
  assert.equal(license.expiry_unix, 1400, 'all frozen time is restored exactly once');
  assert.equal(isLicensePaused(db, license), false);
  assert.equal(remainingSeconds(license, 500), 900);
});
