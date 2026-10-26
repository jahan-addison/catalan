# Catalan Admin

> Licensed under Apache License v2, see details [here](../LICENSE).

* Operator CLI for a catalan database
* Python 3.11+, standard library only

## Overview

The API can't change or delete posts, so the operator works on the database directly, alongside the running server. SQLite's locking makes that safe: WAL lets the server keep reading during a removal, and a write waits for the server's to finish.

There are three commands:

* `list` - browse every post newest first, unlisted ones included
* `show` - print a post and every edit of it
* `remove` - remove a post, and optionally its edits

Waiting for a report means you only see what someone else noticed, so `list` is for reading everything now and then, on your own schedule.

## Commands

The database is `--db`, then `$CATALAN_DB`, then `catalan.db`, the same default as the server.

### list

```bash
python -m catalan_admin list
python -m catalan_admin list --since 2026-10-20
python -m catalan_admin list --unlisted
```

Posts are grouped by day, a page at a time:

```
━━ Page 1 of 7 · posts 1–7 of 49 ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Sun, Oct 18 2026
  23:45  QAaqays8Zck0  unlisted
         free crypto
         send 1 BTC get 2 back, example.com/totally-real

Thu, Oct 15 2026
  03:51  EGwzn1A2GBvP
         Wallis product, by gauss fan
         $$\frac{\pi}{2} = \prod_{n=1}^\infty \frac{4n^2}{4n^2 - 1}$$

↓ next   q quit
```

* `↓`, Enter, or Space - the next page
* `↑` - the previous page
* `q` or Esc - quit
* `--since YYYY-MM-DD` - only posts from that UTC date on, to pick up where the last review stopped
* `--unlisted` - only unlisted posts. No one browses to them, so a report is least likely there
* `--limit N` - posts per page, by default what fits the terminal

Ids are in color, since they're what `show` and `remove` take, and `unlisted` is yellow. Without a terminal, e.g. piped to `grep`, `list` prints every page as plain text. `NO_COLOR=1` turns color off.

### show

```bash
python -m catalan_admin show QAaqays8Zck0
```

Prints every field of the post, a preview of its body, and the ids of its edits.

### remove

```bash
python -m catalan_admin remove QAaqays8Zck0
```

Shows the post and asks for confirmation, `--yes` skips it. A post with edits isn't removed unless you pass `--with-edits`, which also removes the edits of its edits. Everything is removed in one transaction, or nothing is.

A removed post is a `404` on the next request. Browsers revalidate their copy on every view, so it doesn't linger in their caches either.

## Running on the server

The `admin` stage of the root `Dockerfile` is an image with just the CLI. Build and push it with the others:

```bash
docker buildx build --platform linux/amd64 --target admin \
  -t "$IMAGE-admin:$TAG" --push .
```

On the server, mount the same volume as the `catalan` container. `-it` gives `list` the terminal it needs for keys:

```bash
docker run --rm -it -v catalan-data:/data "$IMAGE-admin:$TAG" list
docker run --rm -it -v catalan-data:/data "$IMAGE-admin:$TAG" remove <id>
```

## Development

Poetry manages the development tools, `mypy`, `flake8`, `black`, and `pytest`:

```bash
cd admin
poetry install
make test                                      # mypy --strict, flake8, then pytest
make format                                    # black
poetry run python -m catalan_admin --db ../catalan.db list
```

**Note**: The CLI only opens a database at the schema version it was written for, `SCHEMA_VERSION` in `catalan_admin/database.py`. It must match `k_schema_version` in `catalan/manage.hh`, so bump both together.

### Tests

* `test/fixture/database.py` builds a database with the server's schema, a post, its edits, and a lone post
* `read_key` is tested through a pseudo-terminal, so raw mode and arrow sequences are real
