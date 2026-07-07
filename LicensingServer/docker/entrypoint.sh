#!/usr/bin/env sh
set -eu

mkdir -p /app/backups

python3 scripts/wait_for_db.py

if [ "${LICENSING_SEED_DEMO_DATA:-false}" = "true" ]; then
  python3 scripts/seed_demo_data.py
fi

exec uvicorn licensing_server.main:app \
  --host "${LICENSING_SERVER_HOST:-0.0.0.0}" \
  --port "${LICENSING_SERVER_PORT:-8787}"
