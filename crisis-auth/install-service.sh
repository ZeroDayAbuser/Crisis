#!/bin/sh
set -eu

# Installs crisis-auth as a hardened systemd service.
# Run from this folder with: ./install-service.sh

SERVICE_NAME="${CRISIS_SERVICE_NAME:-crisis-auth}"
SERVICE_PORT="${CRISIS_PORT:-1053}"
NATIVE_PORT="${CRISIS_NATIVE_PORT:-223}"
ENV_FILE="/etc/${SERVICE_NAME}.env"
UNIT_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
DATA_DIR="${CRISIS_DATA_DIR:-/var/lib/${SERVICE_NAME}}"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if [ ! -f "$SCRIPT_DIR/server.js" ] || [ ! -f "$SCRIPT_DIR/package.json" ]; then
  echo "Error: install-service.sh must remain inside the crisis-auth folder." >&2
  exit 1
fi

case "$SCRIPT_DIR" in
  *' '*)
    echo "Error: move crisis-auth to a path without spaces before installing." >&2
    exit 1
    ;;
esac

NODE_BIN=${CRISIS_NODE_BIN:-$(command -v node 2>/dev/null || true)}
NPM_BIN=${CRISIS_NPM_BIN:-$(command -v npm 2>/dev/null || true)}
REQUESTED_SERVICE_USER=${CRISIS_SERVICE_USER:-}

if [ -z "$NODE_BIN" ] || [ ! -x "$NODE_BIN" ]; then
  echo "Error: Node.js was not found. Install Node.js, then rerun this script." >&2
  exit 1
fi

if [ -z "$NPM_BIN" ] || [ ! -x "$NPM_BIN" ]; then
  echo "Error: npm was not found. Install npm, then rerun this script." >&2
  exit 1
fi

# Elevate only the installer. Remember the original account so the Node service
# itself never needs to run as root.
if [ "$(id -u)" -ne 0 ]; then
  INSTALL_USER=$(id -un)
  INSTALL_GROUP=$(id -gn)
  exec sudo env \
    CRISIS_INSTALL_USER="$INSTALL_USER" \
    CRISIS_INSTALL_GROUP="$INSTALL_GROUP" \
    CRISIS_NODE_BIN="$NODE_BIN" \
    CRISIS_NPM_BIN="$NPM_BIN" \
    CRISIS_SERVICE_USER="$REQUESTED_SERVICE_USER" \
    CRISIS_SERVICE_NAME="$SERVICE_NAME" \
    CRISIS_PORT="$SERVICE_PORT" \
    CRISIS_NATIVE_PORT="$NATIVE_PORT" \
    CRISIS_DATA_DIR="$DATA_DIR" \
    sh "$SCRIPT_DIR/install-service.sh"
fi

SERVICE_USER=${CRISIS_SERVICE_USER:-${CRISIS_INSTALL_USER:-${SUDO_USER:-}}}
SERVICE_GROUP=${CRISIS_INSTALL_GROUP:-}

if [ -z "$SERVICE_USER" ]; then
  OWNER=$(stat -c '%U' "$SCRIPT_DIR")
  SERVICE_USER=$OWNER
fi

if [ -z "$SERVICE_GROUP" ]; then
  SERVICE_GROUP=$(id -gn "$SERVICE_USER")
fi

if [ "$SERVICE_USER" = "root" ]; then
  SERVICE_GROUP=root
  echo "Warning: crisis-auth will run as root because root was explicitly selected or owns the deployment folder." >&2
fi

if ! command -v systemctl >/dev/null 2>&1; then
  echo "Error: systemd/systemctl is required." >&2
  exit 1
fi

if ! command -v openssl >/dev/null 2>&1; then
  echo "Error: openssl is required to generate server secrets." >&2
  exit 1
fi

EVENT_SECRET=''
ADMIN_SECRET=''
PREVIOUS_DATABASE_FILE=''
if [ -f "$ENV_FILE" ]; then
  EVENT_SECRET=$(sed -n 's/^CRISIS_EVENT_SECRET=//p' "$ENV_FILE" | head -n 1)
  ADMIN_SECRET=$(sed -n 's/^CRISIS_ADMIN_SECRET=//p' "$ENV_FILE" | head -n 1)
  PREVIOUS_DATABASE_FILE=$(sed -n 's/^DATABASE_PATH=//p' "$ENV_FILE" | head -n 1)
fi

DATABASE_FILE="$DATA_DIR/database.json"
LEGACY_DATABASE="$SCRIPT_DIR/database.json"
install -d -o "$SERVICE_USER" -g "$SERVICE_GROUP" -m 700 "$DATA_DIR"
MIGRATION_SOURCE=''
case "$PREVIOUS_DATABASE_FILE" in
  /*) [ -f "$PREVIOUS_DATABASE_FILE" ] && MIGRATION_SOURCE="$PREVIOUS_DATABASE_FILE" ;;
esac
if [ -z "$MIGRATION_SOURCE" ] && [ -f "$LEGACY_DATABASE" ]; then
  MIGRATION_SOURCE="$LEGACY_DATABASE"
fi
if [ ! -f "$DATABASE_FILE" ] && [ -n "$MIGRATION_SOURCE" ] && [ "$MIGRATION_SOURCE" != "$DATABASE_FILE" ]; then
  install -o "$SERVICE_USER" -g "$SERVICE_GROUP" -m 600 "$MIGRATION_SOURCE" "$DATABASE_FILE"
  echo "Migrated the existing database to persistent storage: $DATABASE_FILE"
fi

# Preserve valid existing secrets so rerunning the installer does not
# unexpectedly invalidate trusted integrations or admin sessions.
if [ "${#EVENT_SECRET}" -lt 32 ]; then
  EVENT_SECRET=$(openssl rand -hex 32)
fi
if [ "${#ADMIN_SECRET}" -lt 32 ]; then
  ADMIN_SECRET=$(openssl rand -hex 32)
fi
while [ "$EVENT_SECRET" = "$ADMIN_SECRET" ]; do
  ADMIN_SECRET=$(openssl rand -hex 32)
done

TEMP_ENV=$(mktemp)
TEMP_UNIT=$(mktemp)
cleanup() {
  rm -f "$TEMP_ENV" "$TEMP_UNIT"
}
trap cleanup EXIT HUP INT TERM

umask 077
{
  printf 'NODE_ENV=production\n'
  printf 'PORT=%s\n' "$SERVICE_PORT"
  printf 'CRISIS_NATIVE_PORT=%s\n' "$NATIVE_PORT"
  printf 'DATABASE_PATH=%s\n' "$DATABASE_FILE"
  printf 'CRISIS_EVENT_SECRET=%s\n' "$EVENT_SECRET"
  printf 'CRISIS_ADMIN_SECRET=%s\n' "$ADMIN_SECRET"
  printf 'CRISIS_CASINO_RATE_WINDOW_MS=10000\n'
  printf 'CRISIS_CASINO_RATE_MAX=30\n'
} > "$TEMP_ENV"
install -o root -g root -m 600 "$TEMP_ENV" "$ENV_FILE"

echo "Installing production Node dependencies..."
if command -v runuser >/dev/null 2>&1; then
  runuser -u "$SERVICE_USER" -- "$NPM_BIN" --prefix "$SCRIPT_DIR" ci --omit=dev
else
  su -s /bin/sh "$SERVICE_USER" -c "cd '$SCRIPT_DIR' && '$NPM_BIN' ci --omit=dev"
fi

if [ -f "$DATABASE_FILE" ]; then
  chown "$SERVICE_USER:$SERVICE_GROUP" "$DATABASE_FILE"
  chmod 600 "$DATABASE_FILE"
fi

echo "Initializing or migrating the persistent database..."
if command -v runuser >/dev/null 2>&1; then
  runuser -u "$SERVICE_USER" -- env DATABASE_PATH="$DATABASE_FILE" "$NODE_BIN" "$SCRIPT_DIR/scripts/migrate.js"
else
  su -s /bin/sh "$SERVICE_USER" -c "DATABASE_PATH='$DATABASE_FILE' '$NODE_BIN' '$SCRIPT_DIR/scripts/migrate.js'"
fi
chown "$SERVICE_USER:$SERVICE_GROUP" "$DATABASE_FILE"
chmod 600 "$DATABASE_FILE"

cat > "$TEMP_UNIT" <<EOF
[Unit]
Description=Crisis authentication and economy service
Wants=network-online.target
After=network-online.target

[Service]
Type=simple
User=$SERVICE_USER
Group=$SERVICE_GROUP
WorkingDirectory=$SCRIPT_DIR
EnvironmentFile=$ENV_FILE
ExecStart=$NODE_BIN $SCRIPT_DIR/server.js
Restart=on-failure
RestartSec=3
TimeoutStopSec=15
UMask=0077
NoNewPrivileges=true
PrivateTmp=true
PrivateDevices=true
ProtectSystem=full
ReadWritePaths=$DATA_DIR
ProtectControlGroups=true
ProtectKernelModules=true
ProtectKernelTunables=true
ProtectKernelLogs=true
LockPersonality=true
RestrictRealtime=true
RestrictSUIDSGID=true
SystemCallArchitectures=native

[Install]
WantedBy=multi-user.target
EOF

install -o root -g root -m 644 "$TEMP_UNIT" "$UNIT_FILE"
systemctl daemon-reload
systemctl enable --now "$SERVICE_NAME.service"

if ! systemctl is-active --quiet "$SERVICE_NAME.service"; then
  echo "Service failed to start. Recent logs:" >&2
  journalctl -u "$SERVICE_NAME.service" -n 50 --no-pager >&2 || true
  exit 1
fi

echo
echo "$SERVICE_NAME is installed, enabled, and running HTTP on port $SERVICE_PORT and native TCP on port $NATIVE_PORT."
echo "Persistent database: $DATABASE_FILE"
echo "Secrets are stored server-side in $ENV_FILE (root-readable only)."
echo "Players do not need to configure anything."
echo
echo "Useful commands:"
echo "  sudo systemctl status $SERVICE_NAME --no-pager"
echo "  sudo journalctl -u $SERVICE_NAME -f"
echo "  sudo systemctl restart $SERVICE_NAME"
