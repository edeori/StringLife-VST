#!/usr/bin/env python3
from __future__ import annotations

import shutil
import sqlite3
import subprocess
import sys
from pathlib import Path
from urllib.parse import urlparse

import psycopg
from licensing_server.config import settings


def sqlite_path() -> Path:
    return Path(settings.database_url.removeprefix("sqlite:///")).expanduser()


def restore_sqlite(backup_path: Path) -> Path:
    target_path = sqlite_path()
    target_path.parent.mkdir(parents=True, exist_ok=True)

    restored_copy = target_path.with_suffix(".restored.sqlite3")
    shutil.copy2(backup_path, restored_copy)

    probe = sqlite3.connect(restored_copy)
    integrity_result = probe.execute("PRAGMA integrity_check").fetchone()
    probe.close()

    if integrity_result is None or integrity_result[0] != "ok":
        restored_copy.unlink(missing_ok=True)
        raise SystemExit("SQLite integrity check failed")

    shutil.move(restored_copy, target_path)
    return target_path


def restore_postgres(backup_path: Path) -> str:
    parsed = urlparse(settings.database_url.replace("+psycopg", ""))
    db_name = parsed.path.lstrip("/")

    if not db_name:
        raise SystemExit("Postgres database name is missing from LICENSING_DATABASE_URL")

    admin_dsn = (
        f"host={parsed.hostname or 'localhost'} "
        f"port={parsed.port or 5432} "
        f"user={parsed.username or ''} "
        f"password={parsed.password or ''} "
        "dbname=postgres"
    )

    with psycopg.connect(admin_dsn, autocommit=True) as connection:
        with connection.cursor() as cursor:
            cursor.execute(
                "SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname = %s AND pid <> pg_backend_pid()",
                (db_name,),
            )
            cursor.execute(f'DROP DATABASE IF EXISTS "{db_name}"')
            cursor.execute(f'CREATE DATABASE "{db_name}"')

    subprocess.run(
        [
            "pg_restore",
            "--no-owner",
            "--dbname",
            settings.database_url,
            str(backup_path),
        ],
        check=True,
    )
    return settings.database_url


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python3 scripts/restore_db.py /path/to/backup")

    backup_path = Path(sys.argv[1]).expanduser()

    if not backup_path.exists():
        raise SystemExit(f"Backup file not found: {backup_path}")

    if settings.database_url.startswith("sqlite:///"):
        print(restore_sqlite(backup_path))
        return

    if settings.database_url.startswith("postgresql"):
        print(restore_postgres(backup_path))
        return

    raise SystemExit(f"Unsupported database URL: {settings.database_url}")


if __name__ == "__main__":
    main()
