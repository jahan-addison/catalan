"""Catalan operator CLI.

Usage:
    python -m catalan_admin show ID
    python -m catalan_admin remove ID [--with-edits] [--yes]

The database path is --db, then CATALAN_DB, then catalan.db, the same default
as the server.
"""

from __future__ import annotations

import os
import sqlite3
import sys
from argparse import ArgumentParser
from datetime import datetime, timezone
from typing import Callable, List, Optional

from catalan_admin.database import Database, Edits_Error, Post, Schema_Error


def format_post(post: Post) -> str:
    """Format a post for the terminal, with a preview of its body.

    Args:
        post: The post.

    Returns:
        A multi-line summary.

    """
    created_at = datetime.fromtimestamp(post.created_at / 1000, tz=timezone.utc)
    preview = post.body if len(post.body) <= 200 else post.body[:200] + "..."
    return "\n".join(
        [
            f"id:         {post.id}",
            f"created_at: {created_at.isoformat(timespec='milliseconds')}",
            f"unlisted:   {'yes' if post.unlisted else 'no'}",
            f"parent_id:  {post.parent_id or '-'}",
            f"title:      {post.title or '-'}",
            f"author:     {post.author or '-'}",
            f"body:       {preview}",
        ]
    )


def show(database: Database, id: str) -> int:
    """Print a post and its edits.

    Args:
        database: The open database.
        id: The post id.

    Returns:
        The exit status, 1 if no post has this id.

    """
    post = database.get_post(id)
    if post is None:
        print(f"no post with id {id}", file=sys.stderr)
        return 1

    print(format_post(post))
    edits = database.get_edits(id)
    print(f"edits:      {', '.join(edits) if edits else '-'}")
    return 0


def remove(
    database: Database,
    id: str,
    with_edits: bool,
    confirm: Callable[[str], bool],
) -> int:
    """Remove a post, and optionally its edits, after confirmation.

    Args:
        database: The open database.
        id: The post id.
        with_edits: Also remove every edit of the post.
        confirm: Asks a yes or no question.

    Returns:
        The exit status, 1 if nothing was removed.

    """
    post = database.get_post(id)
    if post is None:
        print(f"no post with id {id}", file=sys.stderr)
        return 1

    edits = database.get_edits(id)
    if edits and not with_edits:
        print(
            f"{id} has {len(edits)} edit(s): {', '.join(edits)}\n"
            "remove them too with --with-edits",
            file=sys.stderr,
        )
        return 1

    print(format_post(post))
    count = 1 + len(edits)
    if not confirm(f"remove {count} post(s)?"):
        print("nothing removed")
        return 1

    try:
        removed = database.remove_post(id, with_edits=with_edits)
    except Edits_Error as error:
        print(
            f"{id} gained edit(s) since confirming: {', '.join(error.edits)}",
            file=sys.stderr,
        )
        return 1

    for removed_id in removed:
        print(f"removed {removed_id}")
    return 0


def ask(question: str) -> bool:
    """Ask a yes or no question on the terminal, no by default."""
    return input(f"{question} [y/N] ").strip().lower() in ("y", "yes")


def main(argv: Optional[List[str]] = None) -> int:
    """Run the CLI.

    Args:
        argv: Arguments without the program name, sys.argv by default.

    Returns:
        The exit status.

    """
    args_parser = ArgumentParser(prog="catalan_admin")
    args_parser.add_argument(
        "--db",
        dest="db",
        default=os.environ.get("CATALAN_DB", "catalan.db"),
        help="database path (default: $CATALAN_DB, then catalan.db)",
    )
    commands = args_parser.add_subparsers(dest="command", required=True)

    show_parser = commands.add_parser("show", help="print a post and its edits")
    show_parser.add_argument("id")

    remove_parser = commands.add_parser("remove", help="remove a post")
    remove_parser.add_argument("id")
    remove_parser.add_argument(
        "--with-edits",
        action="store_true",
        default=False,
        help="also remove every edit of the post",
    )
    remove_parser.add_argument(
        "-y",
        "--yes",
        action="store_true",
        default=False,
        help="don't ask for confirmation",
    )

    args = args_parser.parse_args(argv)

    try:
        with Database(args.db) as database:
            if args.command == "show":
                return show(database, args.id)
            confirm = (lambda _: True) if args.yes else ask
            return remove(database, args.id, args.with_edits, confirm)
    except (sqlite3.Error, Schema_Error) as error:
        print(f"{args.db}: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
