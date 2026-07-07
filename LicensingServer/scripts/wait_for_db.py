#!/usr/bin/env python3
from __future__ import annotations

import time

from licensing_server.config import settings
from licensing_server.database import check_database_connection


def main() -> None:
    deadline = time.time() + max(5, settings.database_connect_timeout_seconds * 6)
    last_error: Exception | None = None

    while time.time() < deadline:
        try:
            check_database_connection()
            return
        except Exception as exc:  # pragma: no cover - operational retry path
            last_error = exc
            time.sleep(2)

    raise SystemExit(f"Database did not become ready in time: {last_error}")


if __name__ == "__main__":
    main()
