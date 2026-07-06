from __future__ import annotations

import os
from dataclasses import dataclass


@dataclass(frozen=True)
class Settings:
    database_url: str = os.getenv("LICENSING_DATABASE_URL", "sqlite:///./data/licensing.db")
    default_offline_grace_days: int = int(os.getenv("LICENSING_DEFAULT_OFFLINE_GRACE_DAYS", "14"))
    allow_unsigned_development_licenses: bool = os.getenv("LICENSING_ALLOW_UNSIGNED_DEVELOPMENT_LICENSES", "false").lower() in {"1", "true", "yes", "on"}
    signing_private_key: str = os.getenv("LICENSING_SIGNING_PRIVATE_KEY", "").strip()
    signing_key_id: str = os.getenv("LICENSING_SIGNING_KEY_ID", "default").strip() or "default"
    host: str = os.getenv("LICENSING_SERVER_HOST", "127.0.0.1")
    port: int = int(os.getenv("LICENSING_SERVER_PORT", "8787"))


settings = Settings()
