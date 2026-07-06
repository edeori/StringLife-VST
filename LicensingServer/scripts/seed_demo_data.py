from __future__ import annotations

from licensing_server.database import create_db_and_tables


def main() -> None:
    create_db_and_tables()
    print("Database initialized without demo products or demo licenses.")


if __name__ == "__main__":
    main()
