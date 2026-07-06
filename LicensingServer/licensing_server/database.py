from __future__ import annotations

from sqlalchemy import inspect, text
from sqlmodel import Session, SQLModel, create_engine

from .config import settings


connect_args = {"check_same_thread": False} if settings.database_url.startswith("sqlite") else {}
engine = create_engine(settings.database_url, connect_args=connect_args)


def _repair_sqlite_schema() -> None:
    if not settings.database_url.startswith("sqlite"):
        return

    inspector = inspect(engine)

    if "licenserecord" in inspector.get_table_names():
        license_columns = {column["name"] for column in inspector.get_columns("licenserecord")}

        if "comment" not in license_columns:
            with engine.begin() as connection:
                connection.execute(text("ALTER TABLE licenserecord ADD COLUMN comment VARCHAR NOT NULL DEFAULT ''"))


def create_db_and_tables() -> None:
    SQLModel.metadata.create_all(engine)
    _repair_sqlite_schema()


def get_session():
    with Session(engine) as session:
        yield session
