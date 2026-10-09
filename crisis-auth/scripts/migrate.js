const path = require('path');
require('../src/lib/config').loadConfiguration();
const { createStore, CURRENT_SCHEMA_VERSION } = require('../src/lib/data-store');
const databasePath = process.env.DATABASE_PATH ? path.resolve(process.env.DATABASE_PATH) : path.join(__dirname, '..', 'database.json');
const store = createStore(databasePath);
store.initialize();
console.log(`Database migrated to schema ${CURRENT_SCHEMA_VERSION}: ${databasePath}`);
