import sqlite3
from pathlib import Path

import pytest

from catalan_admin import __version__
from catalan_admin.__main__ import main, remove
from catalan_admin.database import Database, Edits_Error, Schema_Error


def count(path: str, table: str) -> int:
    connection = sqlite3.connect(path)
    total: int = connection.execute(f"SELECT count(*) FROM {table}").fetchone()[0]
    connection.close()
    return total


def test_version() -> None:
    assert __version__ == "1.0.2"


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
