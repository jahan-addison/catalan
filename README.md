<div align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/images/logo-on-dark.png">
    <img src="docs/images/logo-on-light.png" width="340" alt="catalan">
  </picture>
</div>

Catalan is a stateless pastebin for math - write Markdown and LaTeX, then share the link. It's my own version of [mathb](https://github.com/susam/mathb), the now discontinued math pastebin.

* C++20 API with Crow and SQLite
* Svelte and TypeScript, built with Vite
* Math by KaTeX, typeset in its own Computer Modern fonts
* Caddy for HTTPS, all in Docker

> 📖 [API reference](api.md) - [Development](development.md)

<div align="center">
  <img src="docs/images/editor.jpg" width="800" alt="Writing a post, with a live preview">
  <br><br>
  <img src="docs/images/post.jpg" width="800" alt="A published post">
</div>

## Building

With Docker:

```bash
docker compose --profile prod up -d --build proxy
open http://localhost:18082
```

To stop it, `docker compose --profile prod rm -sf proxy catalan`. For the Adobe Fonts kit, set `VITE_TYPEKIT_ID` in `frontend/.env.local`.

## Deployment

You need two domains, the site and the API, e.g. `example.com` and `api.example.com`. Point both at your server, and open ports 80 and 443.

Build and push the images:

```bash
export IMAGE=ghcr.io/<you>/catalan
export TAG=$(git rev-parse --short HEAD)

docker buildx build --platform linux/amd64 \
  -t "$IMAGE:$TAG" --push .

docker buildx build --platform linux/amd64 --target proxy \
  --build-arg VITE_TYPEKIT_ID=<your kit id> \
  -t "$IMAGE-proxy:$TAG" --push .
```

On the server:

```bash
export CATALAN_SITE_DOMAIN=example.com
export CATALAN_DOMAIN=api.example.com

docker network create catalan

docker run -d --name catalan \
  --network catalan \
  --restart unless-stopped \
  -e CATALAN_SITE_ORIGIN="https://$CATALAN_SITE_DOMAIN" \
  -v catalan-data:/data \
  "$IMAGE:$TAG"

docker run -d --name catalan-proxy \
  --network catalan \
  --restart unless-stopped \
  -p 80:80 -p 443:443 -p 443:443/udp \
  -e CATALAN_DOMAIN \
  -e CATALAN_SITE_DOMAIN \
  -v caddy-data:/data \
  -v caddy-config:/config \
  "$IMAGE-proxy:$TAG"
```

HTTPS certificates are issued automatically. To deploy a new version, pull the new tag, `docker rm -f` the container, and run it again. The proxy only needs it when the frontend changes. To roll back, run an earlier tag.
