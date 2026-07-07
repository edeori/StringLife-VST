from __future__ import annotations

from pathlib import Path

from sqlalchemy import text
from sqlmodel import Session, SQLModel, create_engine

from .config import settings


connect_args = {}
engine_kwargs = {"connect_args": connect_args, "pool_pre_ping": True}

if settings.database_url.startswith("sqlite"):
    connect_args["check_same_thread"] = False
elif settings.database_url.startswith("postgresql"):
    connect_args["connect_timeout"] = settings.database_connect_timeout_seconds
    engine_kwargs["pool_size"] = settings.database_pool_size
    engine_kwargs["max_overflow"] = settings.database_max_overflow

engine = create_engine(settings.database_url, **engine_kwargs)


def _sqlite_file_path() -> Path | None:
    if not settings.database_url.startswith("sqlite:///"):
        return None

    raw_path = settings.database_url.removeprefix("sqlite:///")
    return Path(raw_path).expanduser()


def _configure_sqlite() -> None:
    if not settings.database_url.startswith("sqlite"):
        return

    db_path = _sqlite_file_path()

    if db_path is not None:
        db_path.parent.mkdir(parents=True, exist_ok=True)

    with engine.begin() as connection:
        connection.execute(text(f"PRAGMA journal_mode={settings.sqlite_journal_mode}"))
        connection.execute(text(f"PRAGMA synchronous={settings.sqlite_synchronous}"))
        connection.execute(text(f"PRAGMA busy_timeout={settings.sqlite_busy_timeout_ms}"))
        connection.execute(text("PRAGMA foreign_keys=ON"))


def create_db_and_tables() -> None:
    _configure_sqlite()
    SQLModel.metadata.create_all(engine)


def check_database_connection() -> None:
    with engine.connect() as connection:
        connection.execute(text("SELECT 1"))


def get_session():
    with Session(engine) as session:
        yield session
