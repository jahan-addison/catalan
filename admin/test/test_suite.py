import io
import os
import pty
import sqlite3
import tty
from pathlib import Path

import pytest

from catalan_admin import __version__
from catalan_admin.__main__ import format_summary, list_posts, main, remove
from catalan_admin.database import Database, Edits_Error, Schema_Error
from catalan_admin.terminal import CLEAR, Style, decode_key, read_key


def count(path: str, table: str) -> int:
    connection = sqlite3.connect(path)
    total: int = connection.execute(f"SELECT count(*) FROM {table}").fetchone()[0]
    connection.close()
    return total


def test_version() -> None:
    assert __version__ == "1.1.0"


def test_get_post(database_path: str) -> None:
    with Database(database_path) as database:
        post = database.get_post("original0000")
        assert post is not None
        assert post.title == "Original"
        assert post.parent_id is None
        assert post.unlisted is False
        assert database.get_post("000000000000") is None


def test_get_edits(database_path: str) -> None:
    with Database(database_path) as database:
        assert database.get_edits("original0000") == [
            "editone00000",
            "editofedit00",
            "edittwo00000",
        ]
        assert database.get_edits("editone00000") == ["editofedit00"]
        assert database.get_edits("lone00000000") == []


def ids(
    path: str, since: int | None = None, unlisted_only: bool = False
) -> list[list[str]]:
    pages = []
    with Database(path) as database:
        after = None
        while True:
            page = database.list_posts(2, after, since, unlisted_only)
            pages.append([post.id for post in page.posts])
            if page.next is None:
                return pages
            after = page.next


def test_list_posts_pages(database_path: str) -> None:
    assert ids(database_path) == [
        ["lone00000000", "edittwo00000"],
        ["editofedit00", "editone00000"],
        ["original0000"],
    ]


def test_list_posts_since_and_unlisted(database_path: str) -> None:
    connection = sqlite3.connect(database_path)
    connection.execute("UPDATE posts SET unlisted = 1 WHERE id = 'editone00000'")
    connection.commit()
    connection.close()
    assert ids(database_path, since=1761275700003) == [["lone00000000", "edittwo00000"]]
    assert ids(database_path, unlisted_only=True) == [["editone00000"]]


def test_list_posts_full_last_page(database_path: str) -> None:
    with Database(database_path) as database:
        page = database.list_posts(5)
        assert len(page.posts) == 5
        assert page.next is None


def test_count_posts(database_path: str) -> None:
    with Database(database_path) as database:
        assert database.count_posts() == 5
        assert database.count_posts(since=1761275700003) == 2
        assert database.count_posts(unlisted_only=True) == 0


def test_format_summary(database_path: str) -> None:
    with Database(database_path) as database:
        post = database.get_post("editone00000")
        assert post is not None
        lines = format_summary(post, Style(False), 80).split("\n")
        assert lines == [
            "  03:15  editone00000  edit of original0000",
            "         (untitled)",
            "         body of editone00000",
        ]


def test_format_summary_fits_the_width(database_path: str) -> None:
    connection = sqlite3.connect(database_path)
    connection.execute(
        "UPDATE posts SET body = ? WHERE id = 'lone00000000'", ("x " * 100,)
    )
    connection.commit()
    connection.close()
    with Database(database_path) as database:
        post = database.get_post("lone00000000")
        assert post is not None
        excerpt = format_summary(post, Style(False), 40).split("\n")[2]
        assert len(excerpt) == 40
        assert excerpt.endswith("...")


def test_style() -> None:
    assert Style(False).id("abc") == "abc"
    assert Style(True).id("abc") == "\033[1;36mabc\033[0m"


def test_list_groups_by_day(
    database_path: str, capsys: pytest.CaptureFixture[str]
) -> None:
    with Database(database_path) as database:
        assert list_posts(database, 2, None, False) == 0
    output = capsys.readouterr().out
    assert CLEAR not in output
    assert "Page 1 of 3 · posts 1–2 of 5" in output
    assert "Page 3 of 3 · posts 5–5 of 5" in output
    # the fixture's posts are all on one day, headed once per page
    assert output.count("Fri, Oct 24 2025") == 3


def test_list_no_posts(database_path: str, capsys: pytest.CaptureFixture[str]) -> None:
    with Database(database_path) as database:
        assert list_posts(database, 2, None, True) == 0
    assert capsys.readouterr().out == "no posts\n"


def browse(path: str, actions: list[str], capsys: pytest.CaptureFixture[str]) -> str:
    remaining = iter(actions)
    with Database(path) as database:
        assert list_posts(database, 2, None, False, lambda: next(remaining)) == 0
    return capsys.readouterr().out


def test_list_browses_forward_and_back(
    database_path: str, capsys: pytest.CaptureFixture[str]
) -> None:
    output = browse(database_path, ["next", "next", "previous", "quit"], capsys)
    pages = [screen.split(" of 3")[0] for screen in output.split(CLEAR)[1:]]
    assert pages == ["━━ Page 1", "━━ Page 2", "━━ Page 3", "━━ Page 2"]


def test_list_ignores_keys_past_either_end(
    database_path: str, capsys: pytest.CaptureFixture[str]
) -> None:
    actions = ["previous", "", "next", "next", "next", "quit"]
    output = browse(database_path, actions, capsys)
    assert output.count(CLEAR) == 3
    assert "↓ next   q quit" in output
    assert "↑ previous   q quit" in output


def test_read_key(monkeypatch: pytest.MonkeyPatch) -> None:
    # a pseudo-terminal, so raw mode and arrow sequences are real
    keyboard, terminal = pty.openpty()
    tty.setraw(terminal)
    stdin = io.TextIOWrapper(io.FileIO(terminal, closefd=False))
    monkeypatch.setattr("sys.stdin", stdin)
    try:
        for typed, action in [
            (b"\033[B", "next"),
            (b"\033[A", "previous"),
            (b"\033", "quit"),
            (b"x", ""),
        ]:
            os.write(keyboard, typed)
            assert read_key() == action
    finally:
        os.close(keyboard)
        os.close(terminal)


def test_decode_key() -> None:
    assert decode_key("\033[B") == "next"
    assert decode_key("\033OB") == "next"
    assert decode_key("\r") == "next"
    assert decode_key("\033[A") == "previous"
    assert decode_key("k") == "previous"
    assert decode_key("q") == "quit"
    assert decode_key("\033") == "quit"
    assert decode_key("x") == ""


def test_main_list(database_path: str, capsys: pytest.CaptureFixture[str]) -> None:
    assert main(["--db", database_path, "list", "--limit", "2"]) == 0
    assert capsys.readouterr().out.endswith("5 post(s)\n")


def test_main_list_bad_arguments(database_path: str) -> None:
    for argv in (["--since", "yesterday"], ["--limit", "0"]):
        with pytest.raises(SystemExit):
            main(["--db", database_path, "list", *argv])


def test_remove_post_alone(database_path: str) -> None:
    with Database(database_path) as database:
        assert database.remove_post("lone00000000") == ["lone00000000"]
        assert database.get_post("lone00000000") is None
    assert count(database_path, "posts") == 4


def test_remove_post_refuses_with_edits(database_path: str) -> None:
    with Database(database_path) as database:
        with pytest.raises(Edits_Error) as error:
            database.remove_post("original0000")
        assert error.value.edits == ["editone00000", "editofedit00", "edittwo00000"]
    assert count(database_path, "posts") == 5
    assert count(database_path, "idempotency_keys") == 1


def test_remove_post_with_edits(database_path: str) -> None:
    with Database(database_path) as database:
        removed = database.remove_post("original0000", with_edits=True)
        assert removed == [
            "original0000",
            "editone00000",
            "editofedit00",
            "edittwo00000",
        ]
    assert count(database_path, "posts") == 1
    assert count(database_path, "idempotency_keys") == 0


def test_remove_post_unknown(database_path: str) -> None:
    with Database(database_path) as database:
        assert database.remove_post("000000000000") == []
    assert count(database_path, "posts") == 5


def test_foreign_keys_are_enabled(database_path: str) -> None:
    with Database(database_path) as database:
        with pytest.raises(sqlite3.IntegrityError):
            database._connection.execute("DELETE FROM posts WHERE id = 'original0000'")


def test_missing_database_is_not_created(tmp_path: Path) -> None:
    path = tmp_path / "missing.db"
    with pytest.raises(sqlite3.OperationalError):
        Database(str(path))
    assert not path.exists()


def test_wrong_schema_version(database_path: str) -> None:
    connection = sqlite3.connect(database_path)
    connection.execute("PRAGMA user_version = 1")
    connection.close()
    with pytest.raises(Schema_Error):
        Database(database_path)


def test_main_show(database_path: str, capsys: pytest.CaptureFixture[str]) -> None:
    assert main(["--db", database_path, "show", "original0000"]) == 0
    output = capsys.readouterr().out
    assert "title:      Original" in output
    assert "editone00000, editofedit00, edittwo00000" in output


def test_main_remove_with_yes(database_path: str) -> None:
    assert main(["--db", database_path, "remove", "lone00000000", "--yes"]) == 0
    assert count(database_path, "posts") == 4


def test_main_remove_refuses_edits(database_path: str) -> None:
    assert main(["--db", database_path, "remove", "original0000", "--yes"]) == 1
    assert count(database_path, "posts") == 5


def test_main_remove_declined(database_path: str) -> None:
    with Database(database_path) as database:
        assert remove(database, "lone00000000", False, lambda _: False) == 1
    assert count(database_path, "posts") == 5


def test_main_missing_database(tmp_path: Path) -> None:
    assert main(["--db", str(tmp_path / "missing.db"), "show", "x"]) == 1
