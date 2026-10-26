# API Reference

Catalan's API stores Markdown and LaTeX posts and gives each one a link. It's served at `https://api.example.com/v1`, and on the site's own origin at `https://example.com/v1`, which is what the frontend uses. Requests and responses are JSON.

* There are no accounts, sessions, or cookies - every request stands on its own
* Posts are immutable - once created, a post never changes, and "editing" one creates a new post with a `parent_id`
* Unlisted posts are left out of the listing, but anyone with the link can read them
* Post ids are random 12-character base62 strings (`0-9`, `a-z`, `A-Z`), so an unlisted link can't be guessed
* Creating posts is for browsers - each new post needs a human check, so reading, listing, and raw access are open to scripts but creating isn't

> 📖 The full specification is [openapi.yaml](openapi.yaml)

---

## Table of Contents

- [Endpoints](#endpoints)
- [Posts](#posts)
- [Listing](#listing)
- [Caching](#caching)
- [Human check](#human-check)
- [Idempotency-Key](#idempotency-key)
- [Errors](#errors)
- [Link previews](#link-previews)
- [Removed posts](#removed-posts)

---

## Endpoints

* `POST /v1/posts` - create a post, or an edit of one with `parent_id`
* `GET /v1/posts` - list posts, newest first, with `limit`, `order`, and `cursor`
* `GET /v1/posts/{id}` - get a post
* `GET /v1/posts/{id}/raw` - get a post's source as plain text
* `GET /health` - `200 ok` when the database is reachable, `503` when it isn't

Create a post, with a token from the [human check](#human-check):

```bash
curl -i https://api.example.com/v1/posts \
  -H 'Content-Type: application/json' \
  -H 'Turnstile-Token: <token>' \
  -d '{ "title": "Euler'\''s identity", "author": "Euler", "body": "$$e^{i\\pi} + 1 = 0$$" }'

# HTTP/1.1 201 Created
# Location: /v1/posts/7HcPq2mVa0Rn
```

Edit it as a new post:

```bash
curl https://api.example.com/v1/posts \
  -H 'Content-Type: application/json' \
  -d '{ "parent_id": "7HcPq2mVa0Rn", "body": "$$e^{i\\pi} = \\cos\\pi + i\\sin\\pi = -1$$" }'
```

Get its source:

```bash
curl https://api.example.com/v1/posts/7HcPq2mVa0Rn/raw
# $$e^{i\pi} + 1 = 0$$
```

---

## Posts

A new post:

* `body` - Markdown with inline `$...$` and display `$$...$$` LaTeX, required, up to 65,536 characters
* `title` - optional, up to 200 characters
* `author` - optional display name, up to 64 characters
* `unlisted` - leave it out of `GET /v1/posts`, `false` by default
* `parent_id` - the post this one is an edit of, which must exist

The server sets `id` and `created_at`, as an ISO 8601 time in UTC:

```json
{
  "id": "7HcPq2mVa0Rn",
  "title": "Euler's identity",
  "author": "Euler",
  "body": "$$e^{i\\pi} + 1 = 0$$",
  "unlisted": false,
  "parent_id": null,
  "created_at": "2026-10-24T03:15:00.123Z"
}
```

Every new database starts with two example posts, Euler's identity at `EulerFormula` and the Basel problem at `BaselProblem`.

---

## Listing

`GET /v1/posts` returns posts newest first, without their bodies. Each has an `excerpt` instead, the first 100 characters of its body with whitespace collapsed, ending in `…` when cut:

```json
{
  "posts": [
    {
      "id": "7HcPq2mVa0Rn",
      "title": "Euler's identity",
      "author": "Euler",
      "parent_id": null,
      "excerpt": "$$e^{i\\pi} + 1 = 0$$",
      "created_at": "2026-10-24T03:15:00.123Z"
    }
  ],
  "next_cursor": "MTc2MTI3NTcwMDEyMy43SGNQcTJtVmEwUm4"
}
```

* `limit` - 1 to 100, 20 by default
* `order` - `desc` (newest first) or `asc`
* `cursor` - the `next_cursor` from the previous page

Pass each response's `next_cursor` as `cursor` until it's `null`:

```bash
curl 'https://api.example.com/v1/posts?limit=20'
curl 'https://api.example.com/v1/posts?limit=20&cursor=<next_cursor>'
```

Cursors are opaque. Posts created while you page appear at the newest end, and never shift a page you haven't read yet, so nothing is skipped or repeated.

---

## Caching

`GET /v1/posts/{id}` and `/raw` send an `ETag` and `Cache-Control: no-cache`. A cached copy is checked on every use: send the `ETag` back as `If-None-Match` to get a `304 Not Modified` with no body:

```bash
curl -i https://api.example.com/v1/posts/7HcPq2mVa0Rn -H 'If-None-Match: "9f2c4e1ab07d3355"'
# HTTP/1.1 304 Not Modified
```

The `ETag` is a hash of the post, so a post the operator corrects or removes is current on the next request.

---

## Human check

Each new post needs a `Turnstile-Token` header, a one-time token from the [Cloudflare Turnstile](https://www.cloudflare.com/products/turnstile/) widget in the browser. The server checks it with Cloudflare before creating the post:

* No token, or one that fails, is a `403` with type `/problems/verification-failed`
* Cloudflare unreachable is a `503`, try again in a moment
* Tokens are single use, so a failed attempt needs a new one
* An [Idempotency-Key](#idempotency-key) replay doesn't need a token, since its first attempt spent it

Creating posts is limited to 10 a minute per IP, within the overall 60 requests a minute.

---

## Idempotency-Key

`POST /v1/posts` isn't idempotent: sending it twice creates two posts. If a connection drops after the server creates a post but before the `201` arrives, a client can't tell whether to retry. An `Idempotency-Key` makes the retry safe:

```bash
KEY="\"$(uuidgen)\""    # one key per post, reused on every retry

curl -i https://api.example.com/v1/posts \
  -H 'Content-Type: application/json' \
  -H "Idempotency-Key: $KEY" \
  -d '{ "body": "$$e^{i\\pi} + 1 = 0$$" }'

# HTTP/1.1 201 Created
# Location: /v1/posts/7HcPq2mVa0Rn
```

Sending it again with the same key returns the same post instead of a new one:

```
HTTP/1.1 201 Created
Location: /v1/posts/7HcPq2mVa0Rn
Idempotent-Replayed: true
```

* The same key with a different post is a `422`, with type `/problems/idempotency-key-reused`
* Keys are kept for 24 hours, and must be 16 to 255 visible ASCII characters
* **Note**: There is no auth, so keys aren't scoped to a client - use a random UUID, never a guessable value

---

## Errors

Errors are [RFC 9457 Problem Details](https://www.rfc-editor.org/rfc/rfc9457) (`application/problem+json`), so every error has the same shape:

```json
{
  "type": "/problems/validation",
  "title": "Validation failed",
  "status": 422,
  "detail": "The request body has 2 invalid fields.",
  "errors": [
    { "field": "body", "message": "must not be empty" },
    { "field": "parent_id", "message": "no post with id 4fQx9ZtLb2Wk" }
  ]
}
```

* `200 OK` - a successful read
* `201 Created` - a post was created, the `Location` header has its URL
* `304 Not Modified` - the cached copy named by `If-None-Match` is current
* `400 Bad Request` - the request can't be parsed, e.g. malformed JSON, `limit=abc`, or a malformed `Idempotency-Key`
* `403 Forbidden` - the human check's token is missing or failed
* `404 Not Found` - no post has this id, a malformed id is treated the same way
* `413 Content Too Large` - the request body is over 256 KiB
* `415 Unsupported Media Type` - the `Content-Type` isn't `application/json`
* `422 Unprocessable Content` - valid JSON that breaks the rules, e.g. an empty `body`, a `parent_id` that doesn't exist, or a reused `Idempotency-Key`
* `429 Too Many Requests` - over 60 requests, or 10 new posts, a minute from one IP, `Retry-After` says how many seconds to wait
* `500 Internal Server Error` - a failure on the server, never the request's fault
* `503 Service Unavailable` - from `/health`, the database is unreachable, or from creating a post, Cloudflare couldn't check the token

**Note**: `413` and `429` come from the proxy as plain text, not Problem Details - go by the status code.

---

## Link previews

Chat apps build link previews from a page's HTML without running JavaScript. For a post URL on the site, the proxy fills in the post's `<title>` and Open Graph tags from the API's `GET /meta/{id}`:

```html
<title>Euler&#39;s identity · catalan</title>
<meta property="og:title" content="Euler&#39;s identity">
<meta property="og:description" content="by Euler · $$e^{i\pi} + 1 = 0$$">
<meta property="og:url" content="https://example.com/7HcPq2mVa0Rn">
```

Unlisted posts include `robots: noindex`. `/meta` is for the site's proxy, not API clients.

---

## Removed posts

Posts can't be deleted through the API. The site's operator can remove a post for abuse or illegal content, see [development.md](development.md#reviewing-and-removing-posts). A removed post is a `404` on the next request, including for a matching `If-None-Match`.
