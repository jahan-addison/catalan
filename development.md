# Development

How to build, test, and operate catalan beyond the quick start in the [README](README.md). The API itself is in [api.md](api.md).

---

## Table of Contents

- [Requirements](#requirements)
- [Dev container](#dev-container)
- [Native](#native)
- [Frontend](#frontend)
- [Example posts](#example-posts)
- [Cross-compiling](#cross-compiling)
- [Configuration](#configuration)
- [Removing a post](#removing-a-post)
- [Dependencies](#dependencies)

---

## Requirements

* Docker Desktop on macOS, Docker Engine 23+ on Linux
* For native builds: CMake 3.16+, a C++20 compiler, SQLite3 headers, and git
* For the frontend: Node 24
* For the admin CLI: Python 3.11+ and Poetry

C++ dependencies are fetched by [CPM](https://github.com/cpm-cmake/CPM.cmake) on the first configure and cached in `~/.cache/CPM`. Set `CPM_SOURCE_CACHE` to change the location.

---

## Dev container

The dev container is Debian at your machine's native architecture, `arm64` on Apple Silicon and `amd64` on most Linux machines. There is no emulation, so builds run at full speed and sanitizers work:

```bash
docker compose up -d --build          # build the toolchain image and start it
docker compose exec dev bash          # shell in /workspace

# inside the container:
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined"
cmake --build build -j
./build/test_suite
./build/catalan                       # http://localhost:18080/health
```

The source tree is bind-mounted, so edit on the host and rebuild in the container. The container's `build/` is a Docker volume, separate from a host `build/`.

To develop on x86_64 from Apple Silicon, run the container under emulation. It's slower, and AddressSanitizer may not work:

```bash
docker compose down -v                # drop the arm64 build volume first
DOCKER_DEFAULT_PLATFORM=linux/amd64 docker compose up -d --build
```

`docker compose down` stops the container, add `-v` to also delete the build and CPM cache volumes.

---

## Native

```bash
# macOS
brew install cmake sqlite

# Ubuntu
sudo apt install build-essential cmake git libsqlite3-dev
```

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZER="Address;Undefined"
cmake --build build -j
./build/test_suite
./build/catalan
```

I personally **do not** recommend developing without at least `Address;Undefined`.

**Note**: The pre-commit `clang-tidy` hook reads `build/compile_commands.json`, so configure a native `build/` even if you develop in the dev container - the container's `build/` is a separate volume.

CMake options:

* `CMAKE_BUILD_TYPE` - `Release` by default, `Debug` adds `-DDEBUG` and debug info
* `USE_SANITIZER` - e.g. `Address;Undefined` or `Thread`
* `CATALAN_BUILD_TESTS` - build `test_suite` and register it with CTest, `ON` by default

---

## Frontend

The site in `frontend/` is Svelte and TypeScript, built with Vite. Run a local catalan server, then the dev server, which forwards `/v1` and `/meta` to it:

```bash
cd frontend
npm install
npm run dev                                   # http://localhost:5173

CATALAN_API_TARGET=http://localhost:18090 npm run dev   # if the dev container has 18080
```

```bash
npm run check                                 # svelte-check, TypeScript
npm test                                      # vitest
npm run build                                 # dist/
npm run screenshots                           # Playwright, every page at desktop and phone widths
```

Posts render with markdown-it and KaTeX, then DOMPurify, and are set in KaTeX's own fonts so prose and math match.

Three build settings, in `frontend/.env.local` or as Docker `--build-arg`s. A `--build-arg` wins when both are set. Start from the template:

```bash
cp frontend/.env.example frontend/.env.local
```


* `VITE_TYPEKIT_ID` - the Adobe Fonts kit for the UI, Neue Haas Grotesk Display and Text, and IBM Plex Mono. Without it, the system fallbacks apply
* `VITE_CONTACT_EMAIL` - where reports go. It adds "Report a post" to the footer and "Report" to each post, opening an email with the post's id and link filled in, and it's the contact on the terms page. Without it, there are no report links
* `VITE_TURNSTILE_SITE_KEY` - the public half of the human check, see [Human check](#human-check)

The terms page is `frontend/src/content/terms.md`, with `{{contact}}` replaced by `VITE_CONTACT_EMAIL`.

### Human check

New posts need a [Cloudflare Turnstile](https://www.cloudflare.com/products/turnstile/) token. Create a widget in the Cloudflare dashboard with your site's domain and `localhost` as hostnames. It gives two keys:

* the **site key**, public, built into the frontend as `VITE_TURNSTILE_SITE_KEY`
* the **secret key**, for the server only, as `CATALAN_TURNSTILE_SECRET` on the `catalan` container. For the compose stack, put it in a `.env` at the project root, from the template:

```bash
cp .env.example .env
```

Without the secret, the server logs a warning and skips the check, which is fine for development. For local testing with the check on, Cloudflare's test keys always pass:

```bash
VITE_TURNSTILE_SITE_KEY=1x00000000000000000000AA \
CATALAN_TURNSTILE_SECRET=1x0000000000000000000000000000000AA \
  docker compose --profile prod up -d --build proxy
```

To see the `403`, use `2x0000000000000000000000000000000AA` as the secret, which always fails.

---

## Example posts

`examples/euler-identity.md` and `examples/basel-problem.md` are the two posts every new database starts with, at `/EulerFormula` and `/BaselProblem`. The server embeds them through a header CMake generates, and the editor shows the Basel problem as its example, so edit them only there.

---

## Cross-compiling

The `builder` stage of the `Dockerfile` always runs on the build machine's native architecture and cross-compiles with Debian's GCC cross toolchain and multiarch SQLite. There is no QEMU involved, so an `amd64` build from an M-series Mac compiles at native speed.

Build a Linux binary without an image:

```bash
docker buildx build --platform linux/amd64 --target artifact --output type=local,dest=dist .
file dist/catalan    # ELF 64-bit LSB pie executable, x86-64 ...
```

The binary links against glibc, libstdc++, and libsqlite3 from Debian trixie - run it on a matching or newer distro, or use the image.

To cross-compile without Docker on a Debian/Ubuntu host, use a toolchain file from `cmake/toolchains/`:

```bash
sudo dpkg --add-architecture arm64 && sudo apt update
sudo apt install crossbuild-essential-arm64 libsqlite3-dev:arm64

cmake -B build-arm64 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64.cmake \
      -DCMAKE_BUILD_TYPE=Release -DCATALAN_BUILD_TESTS=OFF
cmake --build build-arm64 -j
```

**Note**: When cross-compiling, `test_suite` still builds but test discovery is skipped, since the host can't run it.

---

## Configuration

Production is two containers on a private Docker network. `catalan` is the API and SQLite database, never published to the host. `catalan-proxy` is Caddy with the built frontend, the only container on ports 80 and 443, configured in [deploy/Caddyfile](deploy/Caddyfile). It handles HTTPS, rate limiting (60 requests and 10 new posts a minute per IP), request size limits (256 KiB), and compression.

* `CATALAN_PORT` (catalan) - HTTP listen port, `18080` by default
* `CATALAN_DB` (catalan) - SQLite database path, `catalan.db` by default, i.e. `/data/catalan.db` in the volume
* `CATALAN_SITE_ORIGIN` (catalan) - the site's origin for link previews, e.g. `https://example.com`
* `CATALAN_TURNSTILE_SECRET` (catalan) - the Turnstile secret key. Unset, new posts aren't checked
* `CATALAN_DOMAIN` (proxy) - the API's domain. Unset, the proxy serves it on plain HTTP `:80`
* `CATALAN_SITE_DOMAIN` (proxy) - the site's domain. Unset, the proxy serves it on plain HTTP `:8080`
* `CATALAN_UPSTREAM` (proxy) - API address, `catalan:18080` by default

`catalan-data` holds the database and `caddy-data` holds the certificates. **Don't delete `caddy-data`** - Let's Encrypt rate limits certificate issuance. To keep the database in a host directory instead (`-v /srv/catalan:/data`), make it writable by the container user first with `sudo chown 10001:10001 /srv/catalan`.

Without a registry, copy the images over SSH:

```bash
docker buildx build --platform linux/amd64 -t catalan:$TAG --load .
docker buildx build --platform linux/amd64 --target proxy -t catalan-proxy:$TAG --load .
docker save catalan:$TAG catalan-proxy:$TAG | gzip | ssh user@server 'gunzip | docker load'
```

---

## Removing a post

The API can't delete posts. Reports arrive by email through the site's report links, with the post's id in the subject. For abuse or illegal content, the operator CLI in `admin/` removes posts directly from the database, alongside the running server:

```bash
docker buildx build --platform linux/amd64 --target admin \
  -t "$IMAGE-admin:$TAG" --push .

# on the server
docker run --rm -it -v catalan-data:/data "$IMAGE-admin:$TAG" show <id>
docker run --rm -it -v catalan-data:/data "$IMAGE-admin:$TAG" remove <id>
```

`show` prints the post and every edit of it, and `remove` asks for confirmation (`--yes` skips it). A post with edits isn't removed unless you pass `--with-edits`, which also removes the edits of its edits. A removed post is a `404` right away, browsers revalidate their copy on every view.

To work on the CLI itself:

```bash
cd admin
poetry install
make test                                      # mypy, flake8, then pytest
poetry run python -m catalan_admin --db ../catalan.db show <id>
```

---

## Dependencies

C++, fetched by CPM:

* [`Crow`](https://crowcpp.org) - HTTP and routing
* [`asio`](https://think-async.com/Asio/) - Networking for Crow, standalone (no Boost)
* [`sqlite_orm`](https://github.com/fnc12/sqlite_orm) - SQLite ORM
* [`doctest`](https://github.com/doctest/doctest) - Test framework
* [`cmake-scripts`](https://github.com/StableCoder/cmake-scripts) - Sanitizer and tooling integration

Frontend, pinned in `frontend/package.json`:

* [`svelte`](https://svelte.dev) and [`vite`](https://vite.dev)
* [`markdown-it`](https://github.com/markdown-it/markdown-it) with [`markdown-it-texmath`](https://github.com/goessner/markdown-it-texmath)
* [`katex`](https://katex.org)
* [`dompurify`](https://github.com/cure53/DOMPurify)

The admin CLI uses only the Python standard library, with `mypy`, `flake8`, `black`, and `pytest` managed by [Poetry](https://python-poetry.org).
