# syntax=docker/dockerfile:1.7

ARG DEBIAN_RELEASE=trixie

# ##############################################################################
# dev (native arch, source bind-mounted at /workspace via compose.yaml)
# ##############################################################################

FROM debian:${DEBIAN_RELEASE} AS dev

ENV DEBIAN_FRONTEND=noninteractive \
    CPM_SOURCE_CACHE=/root/.cache/CPM

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    clang \
    clang-format \
    clang-tidy \
    cmake \
    cppcheck \
    curl \
    gdb \
    git \
    libsqlite3-dev \
    ninja-build \
    sqlite3 \
    valgrind \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["sleep", "infinity"]

# ##############################################################################
# builder
# ##############################################################################

# Runs on the build host's arch (no QEMU) and cross-compiles for $TARGETARCH,
# or builds natively when the two match
FROM --platform=$BUILDPLATFORM debian:${DEBIAN_RELEASE} AS builder

ARG TARGETARCH
ARG BUILDARCH

ENV DEBIAN_FRONTEND=noninteractive \
    CPM_SOURCE_CACHE=/root/.cache/CPM

RUN set -eux; \
    case "${TARGETARCH}" in amd64|arm64) ;; \
      *) echo "unsupported TARGETARCH: ${TARGETARCH}" >&2; exit 1 ;; \
    esac; \
    if [ "${TARGETARCH}" = "${BUILDARCH}" ]; then \
      toolchain=build-essential; \
    else \
      dpkg --add-architecture "${TARGETARCH}"; \
      toolchain="crossbuild-essential-${TARGETARCH}"; \
    fi; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
      "${toolchain}" ca-certificates cmake git ninja-build \
      "libsqlite3-dev:${TARGETARCH}"; \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN --mount=type=cache,target=/root/.cache/CPM \
    set -eux; \
    toolchain_arg=""; \
    if [ "${TARGETARCH}" != "${BUILDARCH}" ]; then \
      toolchain_arg="-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-${TARGETARCH}.cmake"; \
    fi; \
    cmake -S . -B /build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCATALAN_BUILD_TESTS=OFF \
      ${toolchain_arg}; \
    cmake --build /build; \
    cmake --install /build --strip --prefix /out

# ##############################################################################
# artifact (--target artifact --output type=local,dest=dist)
# ##############################################################################

FROM scratch AS artifact
COPY --from=builder /out/bin/catalan /catalan

# ##############################################################################
# site-builder (the frontend in frontend/, static files for every platform)
# ##############################################################################

FROM --platform=$BUILDPLATFORM node:24.15-slim AS site-builder

# Adobe Fonts kit id, optional. Without it, frontend/.env.local applies, then
# the fallback fonts
ARG VITE_TYPEKIT_ID=""

WORKDIR /site
COPY frontend/package.json frontend/package-lock.json ./
RUN npm ci --no-audit --no-fund
COPY frontend/ ./
COPY examples/ /examples/
# an empty build arg is still an environment variable, which would override
# frontend/.env.local, so it's unset
RUN if [ -z "${VITE_TYPEKIT_ID}" ]; then unset VITE_TYPEKIT_ID; fi; \
    npm run build

# ##############################################################################
# proxy (--target proxy, Caddy with rate limiting and the site, see
# deploy/Caddyfile)
# ##############################################################################

FROM --platform=$BUILDPLATFORM caddy:2.11-builder AS proxy-builder

ARG TARGETOS
ARG TARGETARCH

RUN GOOS=${TARGETOS} GOARCH=${TARGETARCH} xcaddy build \
    --with github.com/mholt/caddy-ratelimit@5625512f24f6f59d6f64fb3aafe5eecff0b286db

FROM caddy:2.11 AS proxy

COPY --from=proxy-builder /usr/bin/caddy /usr/bin/caddy
COPY deploy/Caddyfile /etc/caddy/Caddyfile
COPY --from=site-builder /site/dist /srv/site

# ##############################################################################
# admin (--target admin, the operator CLI in admin/)
# ##############################################################################

# Same uid as runtime, so SQLite's -wal and -shm files stay owned by the server
FROM python:3.14-slim AS admin

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONPATH=/opt/catalan-admin

RUN useradd --system --uid 10001 --home-dir /data catalan

COPY admin/catalan_admin /opt/catalan-admin/catalan_admin

USER catalan
WORKDIR /data

ENTRYPOINT ["python", "-m", "catalan_admin"]

# ##############################################################################
# runtime (default target, buildx resolves it to --platform)
# ##############################################################################

FROM debian:${DEBIAN_RELEASE}-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libsqlite3-0 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --home-dir /data catalan \
    && mkdir -p /data && chown catalan:catalan /data

COPY --from=builder /out/bin/catalan /usr/local/bin/catalan

USER catalan
WORKDIR /data
VOLUME ["/data"]

ENV CATALAN_PORT=18080
EXPOSE 18080

ENTRYPOINT ["/usr/local/bin/catalan"]
