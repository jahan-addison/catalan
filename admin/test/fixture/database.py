import sqlite3
from pathlib import Path

import pytest

# the server's schema at version 2, as sqlite_orm creates it
schema_version_2 = """
CREATE TABLE "idempotency_keys" ("key" TEXT PRIMARY KEY NOT NULL,
    "post_id" TEXT NOT NULL, "created_at" INTEGER NOT NULL,
    FOREIGN KEY("post_id") REFERENCES "posts"("id"));
CREATE TABLE "posts" ("id" TEXT PRIMARY KEY NOT NULL,
    "created_at" INTEGER NOT NULL, "unlisted" INTEGER DEFAULT (0) NOT NULL,
    "parent_id" TEXT NULL, "title" TEXT NULL, "author" TEXT NULL,
    "body" TEXT NOT NULL, FOREIGN KEY("parent_id") REFERENCES "posts"("id"));
CREATE INDEX "idempotency_keys_created_at" ON "idempotency_keys" ("created_at");
CREATE INDEX "posts_listing" ON "posts" ("unlisted", "created_at", "id");
PRAGMA user_version = 2;
"""


def insert_post(
    connection: sqlite3.Connection,
    id: str,
    created_at: int,
    parent_id: str | None = None,
    title: str | None = None,
) -> None:
    connection.execute(
        "INSERT INTO posts (id, created_at, unlisted, parent_id, title, body) "
        "VALUES (?, ?, 0, ?, ?, ?)",
        (id, created_at, parent_id, title, f"body of {id}"),
    )


@pytest.fixture
def database_path(tmp_path: Path) -> str:
    """A schema version 2 database with a post, its edits, and a lone post.

    original                (1761275700000, Idempotency-Key "key-original...")
      edit_one              (+1)
        edit_of_edit        (+2)
      edit_two              (+3)
    lone                    (+4)

    """
    path = tmp_path / "catalan.db"
    connection = sqlite3.connect(path)
    connection.executescript(schema_version_2)
    connection.execute("PRAGMA foreign_keys = ON")
    insert_post(connection, "original0000", 1761275700000, title="Original")
    insert_post(connection, "editone00000", 1761275700001, "original0000")
    insert_post(connection, "editofedit00", 1761275700002, "editone00000")
    insert_post(connection, "edittwo00000", 1761275700003, "original0000")
    insert_post(connection, "lone00000000", 1761275700004)
    connection.execute(
        "INSERT INTO idempotency_keys VALUES (?, ?, ?)",
        ("key-original-0000000000", "original0000", 1761275700000),
    )
    connection.commit()
    connection.close()
    return str(path)
