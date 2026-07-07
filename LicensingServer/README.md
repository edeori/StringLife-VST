# Licensing Server

FastAPI-based licensing backend for JUCE plugins using the reusable client module in
`StringLife/Source/Licensing/`.

This server is intended to become the central licensing service for multiple plugins.
Products are separated by `productId`; the signing key is shared at the server trust-domain
level unless you explicitly decide to rotate or split it later.

## What It Does

- registers products/plugins
- creates and manages license keys
- tracks activations per machine hash
- exposes public licensing API endpoints:
  - `POST /activate`
  - `POST /validate`
  - `POST /refresh`
  - `POST /deactivate`
- exposes an admin UI:
  - `GET /admin`
  - `GET /admin/api/overview`
- signs returned license payloads with a JUCE-compatible RSA private key
- restricts admin routes to allowed CIDR ranges

## Current Architecture

- backend: FastAPI
- database: PostgreSQL
- ORM: SQLModel / SQLAlchemy
- signing: RSA-SHA256
- plugin verification: JUCE `RSAKey` + SHA-256 payload verification

## Trust Model

- the server owns the private signing key
- plugins embed only the public key
- products are separated by `productId`, not by separate signing keys

One central signing keypair is enough for multiple plugins if they all belong to the same
licensing service.

Use separate signing keys only if you want:

- separate trust domains
- plugin-family-specific rotation
- partial incident containment
- stricter operational separation

## Directory Layout

- `licensing_server/`
  - app code
- `scripts/generate_signing_keys.py`
  - generates a JUCE-compatible RSA keypair
- `keys/`
  - local, gitignored key storage for the current signing keypair
- `pgdata/`
  - persistent PostgreSQL data directory
- `backups/`
  - PostgreSQL dump backups
- `.env`
  - local runtime configuration

## Configuration

Environment variables:

- `LICENSING_DATABASE_URL`
- `LICENSING_DATABASE_CONNECT_TIMEOUT_SECONDS`
- `LICENSING_DATABASE_POOL_SIZE`
- `LICENSING_DATABASE_MAX_OVERFLOW`
- `POSTGRES_DB`
- `POSTGRES_USER`
- `POSTGRES_PASSWORD`
- `LICENSING_DEFAULT_OFFLINE_GRACE_DAYS`
- `LICENSING_ALLOW_UNSIGNED_DEVELOPMENT_LICENSES`
- `LICENSING_SIGNING_PRIVATE_KEY`
- `LICENSING_SIGNING_KEY_ID`
- `LICENSING_ADMIN_ALLOWED_CIDRS`
- `LICENSING_TRUST_PROXY_HEADERS`
- `LICENSING_TRUSTED_PROXY_CIDRS`
- `LICENSING_SERVER_HOST`
- `LICENSING_SERVER_PORT`
- `LICENSING_SEED_DEMO_DATA`

Important ones:

- `LICENSING_DATABASE_URL`
  - SQLAlchemy connection string used by both API/admin services
- `POSTGRES_PASSWORD`
  - database password for the bundled Postgres container
- `LICENSING_SIGNING_PRIVATE_KEY`
  - the RSA private key used by the server to sign `signedLicense`
- `LICENSING_ADMIN_ALLOWED_CIDRS`
  - which client networks may access `/admin` and `/admin/api/*`
- `LICENSING_TRUST_PROXY_HEADERS`
  - set to `true` only if the server sits behind a trusted reverse proxy and you want to honor `X-Forwarded-For`
- `LICENSING_TRUSTED_PROXY_CIDRS`
  - which proxy source IPs are trusted to supply forwarding headers

Default admin-allowed networks already include localhost and common RFC1918/private ranges.

## Key Generation

Generate a signing keypair:

```bash
cd LicensingServer
python3 -m venv .venv
source .venv/bin/activate
pip install -e .
python3 scripts/generate_signing_keys.py
```

The script prints two values:

1. plugin-side public key
2. server-side private key

It also writes them locally into:

- `LicensingServer/keys/public_key.txt`
- `LicensingServer/keys/private_key.txt`

That `keys/` directory is gitignored.

Example output shape:

```text
Public key for plugin config (publicKey):
10001,abcd...

Private key for licensing server env (LICENSING_SIGNING_PRIVATE_KEY):
1234...,abcd...
```

Use them like this:

- plugin config:
  - `StringLife/Assets/licensing_config.json`
  - set `publicKey`
- server env:
  - `LicensingServer/.env`
  - set `LICENSING_SIGNING_PRIVATE_KEY`

Important:

- never ship the private key in the plugin
- never commit the private key to git
- only the public key belongs in the plugin

## Plugin Configuration

The plugin config must match the server:

```json
{
  "productId": "stringlife",
  "serverBaseUrl": "https://licenses.example.com",
  "publicKey": "10001,...",
  "allowUnsignedDevelopmentLicenses": false
}
```

During local testing, `serverBaseUrl` can be `http://127.0.0.1:8787`.

## Local Development Setup

### 1. Prepare env

```bash
cd LicensingServer
cp .env.example .env
```

Generate keys and paste the private key into `.env`:

```bash
source .venv/bin/activate
python3 scripts/generate_signing_keys.py
```

Then set at least:

```text
LICENSING_SIGNING_PRIVATE_KEY=...
LICENSING_ALLOW_UNSIGNED_DEVELOPMENT_LICENSES=false
```

### 2. Start server

```bash
mkdir -p backups pgdata
source .venv/bin/activate
docker compose up --build
```

### 3. Open admin

```text
http://127.0.0.1:8788/admin
```

### 4. Register a product

In the admin UI:

- display name: `StringLife`
- product ID: `stringlife`

### 5. Create a license

Either:

- leave key blank to auto-generate

or:

- enter your own key

### 6. Point the plugin to the server

Set in plugin config:

- `serverBaseUrl`: `http://127.0.0.1:8787`
- `productId`: the same value as the registered product, e.g. `stringlife`
- `publicKey`: the generated public key

## Docker

### Build and run

```bash
cd LicensingServer
cp .env.example .env
mkdir -p backups pgdata
docker compose up --build
```

Default compose publishes two listeners:

```text
Public API:  http://127.0.0.1:8787
Admin UI:    http://127.0.0.1:8788/admin
```

Behavior:

- `8787` exposes only the licensing API endpoints
- `8788` exposes only the admin UI and admin actions

Port publishing is configurable through `.env`:

- `LICENSING_API_BIND_ADDRESS`
- `LICENSING_API_EXTERNAL_PORT`
- `LICENSING_ADMIN_BIND_ADDRESS`
- `LICENSING_ADMIN_EXTERNAL_PORT`

Example for a server where the VPN interface address is `10.9.0.4`:

```text
LICENSING_API_BIND_ADDRESS=0.0.0.0
LICENSING_API_EXTERNAL_PORT=8787
LICENSING_ADMIN_BIND_ADDRESS=10.9.0.4
LICENSING_ADMIN_EXTERNAL_PORT=8788
```

With that setup:

- the licensing API is publicly reachable on server port `8787`
- the admin UI listens only on `10.9.0.4:8788`
- the admin panel is not exposed on the public interface

Compose mounts:

- `./pgdata -> /var/lib/postgresql/data`
- `./backups -> /app/backups`

So both the live PostgreSQL cluster and the backup snapshots survive container recreation.

For production, do not expose the app containers directly to the internet without a reverse proxy.

### Deploy Via rsync + SSH

If you do not use git on the server, copy the project directly with `rsync`.

From your local machine:

```bash
cd /Users/mothproduction/Documents/VSCode/StringLife-VST

rsync -avz --delete \
  --exclude '.venv' \
  --exclude '__pycache__' \
  --exclude '*.pyc' \
  --exclude '.env' \
  --exclude 'pgdata' \
  --exclude 'backups' \
  --exclude 'stringlife_licensing_server.egg-info' \
  LicensingServer/ root@207.180.250.28:/opt/licensing-server/
```

Then connect to the server:

```bash
ssh root@207.180.250.28
cd /opt/licensing-server
```

First-time setup on the server:

```bash
cp .env.example .env
mkdir -p pgdata backups
nano .env
docker compose up -d --build
docker compose ps
```

For your VPN-bound admin listener, set at least:

```text
POSTGRES_PASSWORD=replace-this
LICENSING_SIGNING_PRIVATE_KEY=replace-this

LICENSING_API_BIND_ADDRESS=0.0.0.0
LICENSING_API_EXTERNAL_PORT=8787

LICENSING_ADMIN_BIND_ADDRESS=10.9.0.4
LICENSING_ADMIN_EXTERNAL_PORT=8788

LICENSING_ADMIN_ALLOWED_CIDRS=10.9.0.0/24,127.0.0.1/32,::1/128
```

That gives you:

- public API on `207.180.250.28:8787`
- admin UI only on `10.9.0.4:8788`

Useful checks on the server:

```bash
docker compose ps
ss -ltnp | grep -E '8787|8788|5432'
curl http://127.0.0.1:8787/health
```

If you need to push updates later, run the same `rsync` command again, then:

```bash
ssh root@207.180.250.28
cd /opt/licensing-server
docker compose up -d --build
```

## Database Durability And Recovery

This project is now prepared for production with PostgreSQL as the primary database.

### What persists

- live PostgreSQL data:
  - `LicensingServer/pgdata/`
- manual backups:
  - `LicensingServer/backups/`

If the containers restart, the DB is preserved because `pgdata/` is bind-mounted from the host.

### Create a backup

Inside the project directory:

```bash
docker compose exec licensing-api python3 scripts/backup_db.py
```

This writes a timestamped snapshot into:

```text
backups/licensing-YYYYMMDDTHHMMSSZ.dump
```

and also updates:

```text
backups/latest.dump
```

### Restore from backup

Stop the containers first:

```bash
docker compose down
```

Then restore:

```bash
docker run --rm \
  --env-file .env \
  -v "$(pwd)/backups:/app/backups" \
  licensingserver-licensing-api \
  python3 scripts/restore_db.py /app/backups/latest.dump
```

Then start again:

```bash
docker compose up -d
```

### Recommended server-side backup policy

For early production:

- keep `pgdata/` on persistent disk
- run `backup_db.py` on a schedule, for example every hour or every day depending on activity
- sync `backups/` off the server as well
- test restore on a separate machine before relying on it

Example cron on the host:

```cron
0 * * * * cd /opt/licensing-server && docker compose exec -T licensing-api python3 scripts/backup_db.py >/dev/null 2>&1
```

## Admin Network Restriction

Admin routes are restricted by CIDR at the application layer.

Protected paths:

- `/admin`
- `/admin/*`

Example:

```text
LICENSING_ADMIN_ALLOWED_CIDRS=10.10.0.0/16,100.64.0.0/10,127.0.0.1/32,::1/128
```

This is useful when:

- your VPN clients land in a known subnet
- your reverse proxy forwards traffic from a known internal subnet

If you use the split Docker listeners, keep:

- the API listener (`8787`) publicly routable through your reverse proxy
- the admin listener (`8788`) reachable only from VPN/internal networks

### Reverse proxy awareness

If the app runs behind Nginx/Caddy/Traefik and you want the backend to evaluate the original
client IP instead of the proxy IP:

```text
LICENSING_TRUST_PROXY_HEADERS=true
LICENSING_TRUSTED_PROXY_CIDRS=10.0.0.10/32,127.0.0.1/32
```

Only enable this if the proxy itself is trusted and not publicly reachable as an arbitrary
header-forwarding hop.

## Recommended Publishable Topology

This is the direction to take when you publish the plugin and the licensing backend.

### Public API host

Expose a public hostname for plugins:

```text
licenses.example.com
```

Publicly routable endpoints:

- `/activate`
- `/validate`
- `/refresh`
- `/deactivate`
- optionally `/health`

### Private admin host

Expose admin through VPN or internal DNS only:

```text
licenses-admin.internal.example.com
```

or through a private path on the same reverse proxy, but only from VPN CIDRs.

Recommended approach:

- public DNS for plugin API
- separate internal-only DNS name for admin
- WireGuard/Tailscale/OpenVPN for admin access
- route the private hostname to the admin listener port (`8788`)

## Reverse Proxy Strategy

Use a reverse proxy in front of FastAPI.

Responsibilities:

- TLS termination
- rate limiting
- request logging
- public/private route separation
- optional basic auth or SSO for admin

### Nginx example shape

Public API vhost:

- forwards only:
  - `/activate`
  - `/validate`
  - `/refresh`
  - `/deactivate`
  - `/health`

Private admin vhost:

- available only from VPN/internal CIDRs
- forwards to the admin listener port
- forwards:
  - `/admin`
  - `/admin/api/overview`
  - admin POST actions

Even with proxy-level filtering, keep the backend CIDR restriction enabled.

## Suggested Production Hardening

Before public launch, do these:

1. Add schema migrations.
2. Put the app behind Nginx/Caddy/Traefik.
3. Terminate TLS at the proxy.
4. Rate-limit the public licensing endpoints.
5. Add structured logs and backups.
6. Add admin authentication on top of network restriction.
7. Move the private signing key into a real secret store.
8. Rotate keys with a `keyId` migration plan.
9. Add audit logging for activation, refresh, deactivation, disable, delete.

## API Contract

Request:

```json
{
  "licenseKey": "LIC-...",
  "productId": "stringlife",
  "machineId": "sha256-machine-id",
  "pluginVersion": "1.6.2",
  "platform": "macOS",
  "activationId": "...",
  "signedLicense": "{...}"
}
```

Response:

```json
{
  "success": true,
  "status": "activated",
  "activationId": "uuid",
  "signedLicense": "{...}",
  "expiresAt": null,
  "maxActivations": 2,
  "message": "Activation successful"
}
```

## Signed Envelope Format

Returned `signedLicense`:

```json
{
  "algorithm": "rsa-sha256",
  "keyId": "default",
  "payload": "{\"activationId\":\"...\",\"expiresAt\":null,\"issuedAt\":\"2026-07-07T00:00:00Z\",\"lastValidatedAt\":\"2026-07-07T00:00:00Z\",\"licenseKey\":\"...\",\"machineId\":\"...\",\"maxActivations\":2,\"message\":\"License active\",\"offlineGraceDays\":14,\"productId\":\"stringlife\",\"status\":\"activated\"}",
  "signature": "hex-rsa-signature"
}
```

The plugin verifies the SHA-256 digest of `payload` against the configured RSA public key.

## Operational Notes

- `productId` must match between plugin config and server-side product registration
- one central keypair is fine for multiple plugins
- only regenerate/rotate keys when you actually need a trust break or planned migration
- when rotating keys later, use `keyId` and teach the client to trust more than one public key during rollout

## Current State

Good enough for:

- local development
- private beta licensing
- early public rollout behind a proper reverse proxy and VPN-gated admin

Not yet ideal for:

- multi-instance HA deployment
- secret-manager-backed key custody
- enterprise-grade audit/compliance requirements
