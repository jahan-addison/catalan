"""Operator access to the catalan database.

Runs alongside a live server. SQLite's locking makes that safe: WAL lets the
server keep reading during a removal, and a write waits up to the busy timeout
for the server's to finish.
"""

from __future__ import annotations

import sqlite3
from dataclasses import dataclass
from types import TracebackType
from typing import List, Optional, Tuple, Type

# catalan::orm::k_schema_version, the schema the queries below are written for
SCHEMA_VERSION = 2


class Schema_Error(Exception):
    __module__ = "Catalan Admin"


class Edits_Error(Exception):
    """A post can't be removed alone, other posts are edits of it.

    Attributes:
        edits: Ids of every post descended from it, oldest first.

    """

    __module__ = "Catalan Admin"

    def __init__(self, edits: List[str]) -> None:
        self.edits = edits
        super().__init__(f"{len(edits)} edit(s) depend on this post")


@dataclass(frozen=True)
class Post:
    """A row of the posts table."""

    id: str
    created_at: int
    unlisted: bool
    parent_id: Optional[str]
    title: Optional[str]
    author: Optional[str]
    body: str


# (created_at, id) of the last post on a page, where the next one starts
Cursor = Tuple[int, str]


@dataclass(frozen=True)
class Page:
    """One page of posts, newest first.

    Attributes:
        posts: The posts on this page.
        next: Where the next page starts, or None on the last page.

    """

    posts: List[Post]
    next: Optional[Cursor]


def filters(since: Optional[int], unlisted_only: bool) -> List[str]:
    """Build the SQL conditions shared by list_posts and count_posts.

    Args:
        since: Only posts created at or after this time, bound as :since.
        unlisted_only: Only unlisted posts.

    Returns:
        The conditions, empty for every post.

    """
    conditions = []
    if since is not None:
        conditions.append("created_at >= :since")
    if unlisted_only:
        conditions.append("unlisted = 1")
    return conditions


class Database:
    """Connection to an existing catalan database.

    Opens read-write without creating the file, so a mistyped path fails
    instead of making an empty database. Foreign keys are enabled, since
    Python's sqlite3 leaves them off, and the schema version must match
    SCHEMA_VERSION.

    Args:
        path: Path to the database file.
        timeout: Seconds a write waits for the server's lock.

    Raises:
        sqlite3.OperationalError: The file doesn't exist or can't be opened.
        Schema_Error: The database isn't at SCHEMA_VERSION.

    """

    def __init__(self, path: str, timeout: float = 5.0) -> None:
        self._connection = sqlite3.connect(
            f"file:{path}?mode=rw", uri=True, timeout=timeout, isolation_level=None
        )
        self._connection.execute("PRAGMA foreign_keys = ON")
        version = self._connection.execute("PRAGMA user_version").fetchone()[0]
        if version != SCHEMA_VERSION:
            self._connection.close()
            raise Schema_Error(
                f"database is at schema version {version}, "
                f"this tool is for version {SCHEMA_VERSION}"
            )

    def __enter__(self) -> Database:
        return self

    def __exit__(
        self,
        kind: Optional[Type[BaseException]],
        value: Optional[BaseException],
        traceback: Optional[TracebackType],
    ) -> None:
        self.close()

    def close(self) -> None:
        """Close the connection."""
        self._connection.close()

    def get_post(self, id: str) -> Optional[Post]:
        """Get a post by id.

        Args:
            id: The post id.

        Returns:
            The post, or None if no post has this id.

        """
        row = self._connection.execute(
            "SELECT id, created_at, unlisted, parent_id, title, author, body "
            "FROM posts WHERE id = ?",
            (id,),
        ).fetchone()
        if row is None:
            return None
        return Post(row[0], row[1], bool(row[2]), row[3], row[4], row[5], row[6])

    def list_posts(
        self,
        limit: int,
        after: Optional[Cursor] = None,
        since: Optional[int] = None,
        unlisted_only: bool = False,
    ) -> Page:
        """List posts newest first, unlisted ones included.

        Pages are keyset pagination over (created_at, id), the same as the
        API's listing, so a post created while paging never shifts a page.
        One extra row is read to know whether another page follows.

        Args:
            limit: Posts per page.
            after: The previous page's next, None for the first page.
            since: Only posts created at or after this time, in epoch ms.
            unlisted_only: Only unlisted posts.

        Returns:
            The page.

        """
        conditions = [
            "(:after_at IS NULL OR (created_at, id) < (:after_at, :after_id))",
            *filters(since, unlisted_only),
        ]
        rows = self._connection.execute(
            "SELECT id, created_at, unlisted, parent_id, title, author, body "
            f"FROM posts WHERE {' AND '.join(conditions)} "
            "ORDER BY created_at DESC, id DESC LIMIT :limit",
            {
                "after_at": after[0] if after else None,
                "after_id": after[1] if after else None,
                "since": since,
                "limit": limit + 1,
            },
        ).fetchall()
        posts = [
            Post(row[0], row[1], bool(row[2]), row[3], row[4], row[5], row[6])
            for row in rows[:limit]
        ]
        last = posts[-1] if len(rows) > limit else None
        return Page(posts, (last.created_at, last.id) if last else None)

    def count_posts(
        self, since: Optional[int] = None, unlisted_only: bool = False
    ) -> int:
        """Count the posts list_posts pages through.

        Args:
            since: Only posts created at or after this time, in epoch ms.
            unlisted_only: Only unlisted posts.

        Returns:
            The number of posts.

        """
        conditions = filters(since, unlisted_only) or ["1"]
        total: int = self._connection.execute(
            f"SELECT count(*) FROM posts WHERE {' AND '.join(conditions)}",
            {"since": since},
        ).fetchone()[0]
        return total

    def get_edits(self, id: str) -> List[str]:
        """Get every post descended from a post.

        Edits of edits are included, found with a recursive query that walks
        parent_id down from the post.

        Args:
            id: The post id.

        Returns:
            Ids of the descendants, oldest first, not including the post.

        """
        rows = self._connection.execute(
            "WITH RECURSIVE descendants(id, created_at) AS ("
            "  SELECT id, created_at FROM posts WHERE parent_id = ?"
            "  UNION"
            "  SELECT posts.id, posts.created_at FROM posts"
            "  JOIN descendants ON posts.parent_id = descendants.id"
            ") SELECT id FROM descendants ORDER BY created_at, id",
            (id,),
        ).fetchall()
        return [row[0] for row in rows]

    def remove_post(self, id: str, with_edits: bool = False) -> List[str]:
        """Remove a post, and optionally every edit of it.

        Removes the post's Idempotency-Keys first, since they reference it.
        Everything is removed in one transaction, or nothing is.

        Args:
            id: The post id.
            with_edits: Also remove every post descended from it.

        Returns:
            Ids of the removed posts, the post first. Empty if no post has
            this id.

        Raises:
            Edits_Error: The post has edits and with_edits is False.

        """
        # IMMEDIATE takes the write lock up front, so no edit can be added
        # between finding the post's edits and removing them
        self._connection.execute("BEGIN IMMEDIATE")
        try:
            if self.get_post(id) is None:
                self._connection.execute("ROLLBACK")
                return []

            edits = self.get_edits(id)
            if edits and not with_edits:
                raise Edits_Error(edits)

            ids = [id, *edits]
            placeholders = ", ".join("?" for _ in ids)
            self._connection.execute(
                f"DELETE FROM idempotency_keys WHERE post_id IN ({placeholders})",
                ids,
            )
            self._connection.execute(
                f"DELETE FROM posts WHERE id IN ({placeholders})", ids
            )
            self._connection.execute("COMMIT")
        except BaseException:
            self._connection.execute("ROLLBACK")
            raise

        return ids
