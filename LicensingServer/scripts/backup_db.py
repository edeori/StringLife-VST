#!/usr/bin/env python3
from __future__ import annotations

import os
import shutil
import sqlite3
import subprocess
from datetime import datetime, timezone
from pathlib import Path

from licensing_server.config import settings


def sqlite_path() -> Path:
    return Path(settings.database_url.removeprefix("sqlite:///")).expanduser()


def backup_dir() -> Path:
    path = Path("./backups").resolve()
    path.mkdir(parents=True, exist_ok=True)
    return path


def backup_sqlite() -> Path:
    source_path = sqlite_path()

    if not source_path.exists():
        raise SystemExit(f"Database file not found: {source_path}")

    target_dir = backup_dir()
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    backup_path = target_dir / f"licensing-{timestamp}.sqlite3"
    latest_path = target_dir / "latest.sqlite3"

    source = sqlite3.connect(source_path)
    destination = sqlite3.connect(backup_path)

    try:
        source.backup(destination)
    finally:
        destination.close()
        source.close()

    shutil.copy2(backup_path, latest_path)
    return backup_path


def backup_postgres() -> Path:
    target_dir = backup_dir()
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    backup_path = target_dir / f"licensing-{timestamp}.dump"
    latest_path = target_dir / "latest.dump"

    env = os.environ.copy()
    env.setdefault("PGCONNECT_TIMEOUT", str(settings.database_connect_timeout_seconds))

    subprocess.run(
        [
            "pg_dump",
            "--format=custom",
            "--no-owner",
            "--file",
            str(backup_path),
            settings.database_url,
        ],
        check=True,
        env=env,
    )

    shutil.copy2(backup_path, latest_path)
    return backup_path


def main() -> None:
    if settings.database_url.startswith("sqlite:///"):
        print(backup_sqlite())
        return

    if settings.database_url.startswith("postgresql"):
        print(backup_postgres())
        return

    raise SystemExit(f"Unsupported database URL: {settings.database_url}")


if __name__ == "__main__":
    main()
