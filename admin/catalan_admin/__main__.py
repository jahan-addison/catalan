"""Catalan operator CLI.

Usage:
    python -m catalan_admin list [--limit N] [--since YYYY-MM-DD] [--unlisted]
    python -m catalan_admin show ID
    python -m catalan_admin remove ID [--with-edits] [--yes]

The database path is --db, then CATALAN_DB, then catalan.db, the same default
as the server.
"""

from __future__ import annotations

import os
import shutil
import sqlite3
import sys
from argparse import ArgumentParser, ArgumentTypeError
from datetime import date, datetime, time, timezone
from typing import Callable, List, Optional

from catalan_admin.database import (
    Cursor,
    Database,
    Edits_Error,
    Page,
    Post,
    Schema_Error,
)
from catalan_admin.terminal import CLEAR, Style, read_key


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


def format_summary(post: Post, style: Style, width: int) -> str:
    """Format a post as a listing entry, under its day's heading.

    Args:
        post: The post.
        style: Terminal styling.
        width: Terminal width, the excerpt is cut to fit.

    Returns:
        Three lines: time, id, and flags; title and author; the excerpt.

    """
    created_at = datetime.fromtimestamp(post.created_at / 1000, tz=timezone.utc)
    flags = []
    if post.unlisted:
        flags.append(style.warning("unlisted"))
    if post.parent_id:
        flags.append(style.link(f"edit of {post.parent_id}"))

    title = style.bold(post.title) if post.title else style.dim("(untitled)")
    if post.author:
        title += style.dim(f", by {post.author}")

    indent = " " * 9
    excerpt = " ".join(post.body.split())
    room = max(width - len(indent), 20)
    if len(excerpt) > room:
        excerpt = excerpt[: room - 3] + "..."

    head = f"  {style.dim(f'{created_at:%H:%M}')}  {style.id(post.id)}"
    if flags:
        head += "  " + style.dim(" · ").join(flags)
    return "\n".join([head, f"{indent}{title}", f"{indent}{excerpt}"])


def format_day(created_at: int, style: Style) -> str:
    """Format the heading for a day's posts, e.g. "Thu, Oct 22 2026"."""
    day = datetime.fromtimestamp(created_at / 1000, tz=timezone.utc)
    return style.bold(f"{day:%a, %b %d %Y}")


def format_rule(text: str, style: Style, width: int) -> str:
    """Format a page heading as a rule across the terminal."""
    line = f"━━ {text} "
    return style.dim(line + "━" * max(width - len(line), 0))


def format_page(
    page: Page,
    number: int,
    pages: int,
    first: int,
    total: int,
    style: Style,
    width: int,
) -> str:
    """Format one page of the listing, its rule and then its posts by day.

    Args:
        page: The page.
        number: The page's number, from 1.
        pages: The number of pages.
        first: The position of the page's first post, from 1.
        total: The number of posts.
        style: Terminal styling.
        width: Terminal width.

    Returns:
        The page's lines.

    """
    last = first + len(page.posts) - 1
    lines = [
        format_rule(
            f"Page {number} of {pages} · posts {first}–{last} of {total}",
            style,
            width,
        )
    ]
    day = ""
    for post in page.posts:
        heading = format_day(post.created_at, style)
        if heading != day:
            lines += ["", heading]
            day = heading
        lines.append(format_summary(post, style, width))
    return "\n".join(lines) + "\n"


def format_keys(number: int, pages: int, style: Style) -> str:
    """Format the keys that work on this page, e.g. no previous on page 1."""
    keys = []
    if number < pages:
        keys.append("↓ next")
    if number > 1:
        keys.append("↑ previous")
    keys.append("q quit")
    return style.dim("   ".join(keys))


def list_posts(
    database: Database,
    limit: int,
    since: Optional[int],
    unlisted_only: bool,
    keys: Optional[Callable[[], str]] = None,
    style: Style = Style(False),
    width: int = 80,
) -> int:
    """Print every post newest first, a page at a time, grouped by day.

    With keys, the pages are browsed on a terminal: each one replaces the
    last, and keys move to the next or previous page. Without, every page
    is printed in order.

    Pages are keyset pagination, which only moves forward, so going back
    reuses the start of a page already shown.

    Args:
        database: The open database.
        limit: Posts per page.
        since: Only posts created at or after this time, in epoch ms.
        unlisted_only: Only unlisted posts.
        keys: Reads the next key's action, see terminal.read_key.
        style: Terminal styling.
        width: Terminal width.

    Returns:
        The exit status, always 0.

    """
    total = database.count_posts(since, unlisted_only)
    if total == 0:
        print("no posts")
        return 0

    pages = -(-total // limit)
    # starts[n] is where page n + 1 starts, None for the first page
    starts: List[Optional[Cursor]] = [None]
    while True:
        number = len(starts)
        page = database.list_posts(limit, starts[-1], since, unlisted_only)
        text = format_page(
            page, number, pages, (number - 1) * limit + 1, total, style, width
        )

        if keys is None:
            print(text)
            if page.next is None:
                break
            starts.append(page.next)
            continue

        print(CLEAR + text)
        print(format_keys(number, pages, style), end="", flush=True)
        action = ""
        while not action:
            action = keys()
            if action == "next" and page.next is None:
                action = ""
            if action == "previous" and number == 1:
                action = ""
        if action == "quit":
            print()
            return 0
        if action == "next":
            starts.append(page.next)
        else:
            starts.pop()

    print(style.dim(f"{total} post(s)"))
    return 0


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


def epoch_ms(day: str) -> int:
    """Parse a YYYY-MM-DD date as the start of that day in UTC, in epoch ms.

    Args:
        day: The date.

    Returns:
        Milliseconds since the epoch.

    Raises:
        ArgumentTypeError: The date isn't YYYY-MM-DD.

    """
    try:
        start = datetime.combine(date.fromisoformat(day), time(), tzinfo=timezone.utc)
    except ValueError:
        raise ArgumentTypeError(f"not a YYYY-MM-DD date: {day}") from None
    return int(start.timestamp() * 1000)


def page_size(value: str) -> int:
    """Parse a positive number of posts per page.

    Raises:
        ArgumentTypeError: The value isn't a positive integer.

    """
    if not value.isdigit() or int(value) < 1:
        raise ArgumentTypeError(f"not a positive number: {value}")
    return int(value)


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

    list_parser = commands.add_parser(
        "list", help="page through every post, newest first, unlisted included"
    )
    list_parser.add_argument(
        "--limit",
        type=page_size,
        default=None,
        help="posts per page (default: what fits the terminal, or 20 piped)",
    )
    list_parser.add_argument(
        "--since",
        type=epoch_ms,
        default=None,
        help="only posts created on or after this UTC date, YYYY-MM-DD",
    )
    list_parser.add_argument(
        "--unlisted",
        action="store_true",
        default=False,
        help="only unlisted posts",
    )

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
            if args.command == "list":
                size = shutil.get_terminal_size()
                interactive = sys.stdin.isatty() and sys.stdout.isatty()
                # a post is at most 5 lines with its day heading, and the
                # rule and keys take 3
                limit = args.limit or (
                    max((size.lines - 3) // 5, 1) if interactive else 20
                )
                return list_posts(
                    database,
                    limit,
                    args.since,
                    args.unlisted,
                    read_key if interactive else None,
                    Style.for_stream(sys.stdout),
                    size.columns,
                )
            if args.command == "show":
                return show(database, args.id)
            confirm = (lambda _: True) if args.yes else ask
            return remove(database, args.id, args.with_edits, confirm)
    except (sqlite3.Error, Schema_Error) as error:
        print(f"{args.db}: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
