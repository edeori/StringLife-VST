# Licensing Server Scaffold

This is a FastAPI + SQLite licensing server that matches the JUCE client contract in `StringLife/Source/Licensing/`.

## What it does

- `POST /activate`
- `POST /validate`
- `POST /refresh`
- `POST /deactivate`
- stores licenses and activations in SQLite
- enforces `maxActivations`
- returns a signed `signedLicense` envelope using a JUCE-compatible RSA keypair

This is suitable for local integration and workflow testing with the current plugin client config:

- the plugin can activate against this server
- the plugin can validate and refresh while offline grace is still within policy
- the plugin can deactivate an activation
- `/admin` shows a small browser UI with issued licenses and registered machine activations

## Quick start

### Native Python

```bash
cd LicensingServer
python3 -m venv .venv
source .venv/bin/activate
pip install -e .
cp .env.example .env
python3 scripts/generate_signing_keys.py
# copy the printed private key into LICENSING_SIGNING_PRIVATE_KEY in .env
python3 scripts/seed_demo_data.py
uvicorn licensing_server.main:app --reload --port 8787
```

Then open:

```text
http://127.0.0.1:8787/admin
```

### Docker

```bash
cd LicensingServer
cp .env.example .env
python3 scripts/generate_signing_keys.py
# paste LICENSING_SIGNING_PRIVATE_KEY into .env
mkdir -p data
docker compose up --build
```

This exposes the server on:

```text
http://127.0.0.1:8787
```

and keeps the SQLite database in:

```text
LicensingServer/data/licensing.db
```

Point the plugin config to:

```json
{
  "serverBaseUrl": "http://127.0.0.1:8787",
  "publicKey": "PASTE_THE_GENERATED_JUCE_PUBLIC_KEY_HERE",
  "allowUnsignedDevelopmentLicenses": false
}
```

## Signing

The plugin verifies license envelopes with the `publicKey` from its licensing config.
The server signs payloads with `LICENSING_SIGNING_PRIVATE_KEY`.

The bundled key script prints both values in the JUCE RSA format:

```bash
python3 scripts/generate_signing_keys.py
```

This prints:

- plugin-side `publicKey`
- server-side `LICENSING_SIGNING_PRIVATE_KEY`

If you explicitly want unsigned local development envelopes, set:

```text
LICENSING_ALLOW_UNSIGNED_DEVELOPMENT_LICENSES=true
```

and set the plugin config to:

```json
{
  "allowUnsignedDevelopmentLicenses": true
}
```

## Docker notes

- The container runs `uvicorn` directly.
- SQLite persistence is handled by mounting `./data` into `/app/data`.
- For local POC this is fine.
- For production you would typically switch to Postgres and run the app behind a reverse proxy/load balancer.

## Data model

- `LicenseRecord`
  - one purchased license key
  - product id
  - max activations
  - enabled / revoked flags
- `ActivationRecord`
  - one machine activation for one license
  - machine id hash
  - platform
  - plugin version
  - activated at
  - last validated at
  - deactivated at

## Expected request shape

```json
{
  "licenseKey": "SL-DEV-0001",
  "productId": "stringlife",
  "machineId": "sha256-machine-id",
  "pluginVersion": "1.6.2",
  "platform": "macOS"
}
```

For refresh / validate / deactivate:

```json
{
  "licenseKey": "SL-DEV-0001",
  "productId": "stringlife",
  "machineId": "sha256-machine-id",
  "pluginVersion": "1.6.2",
  "platform": "macOS",
  "activationId": "uuid",
  "signedLicense": "{...}"
}
```

## Signed payload shape

The server returns a `signedLicense` JSON envelope:

```json
{
  "algorithm": "rsa-sha256",
  "keyId": "default",
  "payload": "{\"activationId\":\"...\",\"expiresAt\":null,\"issuedAt\":\"2026-07-07T00:00:00Z\",\"lastValidatedAt\":\"2026-07-07T00:00:00Z\",\"licenseKey\":\"...\",\"machineId\":\"...\",\"maxActivations\":2,\"message\":\"License active\",\"offlineGraceDays\":14,\"productId\":\"stringlife\",\"status\":\"activated\"}",
  "signature": "hex-rsa-signature"
}
```

The plugin verifies the SHA-256 digest of `payload` with the configured RSA public key.

## Suggested production roadmap

1. Move from SQLite to Postgres and add schema migrations.
2. Add authentication and authorization for the admin surface.
3. Add audit logs for activation, validation, refresh, deactivation, and suspicious failures.
4. Add TLS, rate limiting, backups, monitoring, and key rotation.
5. Split admin UI and public licensing API if you want stricter perimeter control.

## Is FastAPI OK for production?

Yes. FastAPI itself is a reasonable production choice for this type of internal API.

What matters is the surrounding system:

- run behind a real reverse proxy or load balancer
- terminate TLS properly
- move from SQLite to Postgres or another production database
- add authentication for admin surfaces
- add migrations, monitoring, backups, and audit logs
- add request validation, rate limiting, and abuse controls

So the answer is:

- for local POC: this implementation is fine
- for production: FastAPI is still fine, but this exact scaffold is not production-ready until the signing, auth, DB, and ops pieces are hardened
