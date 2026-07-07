from __future__ import annotations

import os
from dataclasses import dataclass


def _env_bool(name: str, default: str = "false") -> bool:
    return os.getenv(name, default).strip().lower() in {"1", "true", "yes", "on"}


def _env_csv(name: str, default: str) -> tuple[str, ...]:
    raw = os.getenv(name, default)
    return tuple(part.strip() for part in raw.split(",") if part.strip())


@dataclass(frozen=True)
class Settings:
    database_url: str = os.getenv("LICENSING_DATABASE_URL", "postgresql+psycopg://licensing:change-me@postgres:5432/licensing")
    database_connect_timeout_seconds: int = int(os.getenv("LICENSING_DATABASE_CONNECT_TIMEOUT_SECONDS", "10"))
    database_pool_size: int = int(os.getenv("LICENSING_DATABASE_POOL_SIZE", "10"))
    database_max_overflow: int = int(os.getenv("LICENSING_DATABASE_MAX_OVERFLOW", "20"))
    sqlite_busy_timeout_ms: int = int(os.getenv("LICENSING_SQLITE_BUSY_TIMEOUT_MS", "5000"))
    sqlite_journal_mode: str = os.getenv("LICENSING_SQLITE_JOURNAL_MODE", "WAL").strip() or "WAL"
    sqlite_synchronous: str = os.getenv("LICENSING_SQLITE_SYNCHRONOUS", "NORMAL").strip() or "NORMAL"
    default_offline_grace_days: int = int(os.getenv("LICENSING_DEFAULT_OFFLINE_GRACE_DAYS", "14"))
    allow_unsigned_development_licenses: bool = _env_bool("LICENSING_ALLOW_UNSIGNED_DEVELOPMENT_LICENSES")
    signing_private_key: str = os.getenv("LICENSING_SIGNING_PRIVATE_KEY", "").strip()
    signing_key_id: str = os.getenv("LICENSING_SIGNING_KEY_ID", "default").strip() or "default"
    expose_public_api: bool = _env_bool("LICENSING_EXPOSE_PUBLIC_API", "true")
    expose_admin_ui: bool = _env_bool("LICENSING_EXPOSE_ADMIN_UI", "true")
    admin_allowed_cidrs: tuple[str, ...] = _env_csv(
        "LICENSING_ADMIN_ALLOWED_CIDRS",
        "127.0.0.1/32,::1/128,10.0.0.0/8,172.16.0.0/12,192.168.0.0/16,100.64.0.0/10",
    )
    trust_proxy_headers: bool = _env_bool("LICENSING_TRUST_PROXY_HEADERS")
    trusted_proxy_cidrs: tuple[str, ...] = _env_csv(
        "LICENSING_TRUSTED_PROXY_CIDRS",
        "127.0.0.1/32,::1/128",
    )
    host: str = os.getenv("LICENSING_SERVER_HOST", "127.0.0.1")
    port: int = int(os.getenv("LICENSING_SERVER_PORT", "8787"))


settings = Settings()
