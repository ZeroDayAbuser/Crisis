const fs = require('fs');
const path = require('path');
require('./src/lib/config').loadConfiguration();

if (require.main === module) {
  if (!process.env.DATABASE_PATH) {
    throw new Error('DATABASE_PATH is required. Point it at the persistent license database before starting the server.');
  }
  if (!path.isAbsolute(process.env.DATABASE_PATH)) {
    throw new Error('DATABASE_PATH must be absolute so changing the working directory cannot select another database.');
  }
  if (!fs.existsSync(process.env.DATABASE_PATH)) {
    throw new Error('DATABASE_PATH does not exist. Restore the persistent database or run `npm run migrate` to initialize a new one.');
  }
}

const service = require('./src/app');

if (require.main === module) service.startServer();

module.exports = service;
