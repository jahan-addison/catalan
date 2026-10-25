import { svelte } from "@sveltejs/vite-plugin-svelte";
import { defineConfig, loadEnv, type Plugin } from "vite";

/**
 * @brief Link the Adobe Fonts kit when VITE_TYPEKIT_ID is set
 *
 * Without a kit, the fallback font stacks in app.css apply.
 */
function typekit(kit: string | undefined): Plugin {
  return {
    name: "catalan-typekit",
    transformIndexHtml() {
      if (!kit) return [];
      return [
        {
          tag: "link",
          attrs: { rel: "preconnect", href: "https://use.typekit.net", crossorigin: "" },
          injectTo: "head",
        },
        {
          tag: "link",
          attrs: { rel: "stylesheet", href: `https://use.typekit.net/${kit}.css` },
          injectTo: "head",
        },
      ];
    },
  };
}

/**
 * @brief Turn the default meta block into a Caddy template, at build only
 *
 * For a post URL the proxy includes the post's tags from the API's
 * /meta/{id}, so link previews show the post. Any other page keeps the
 * defaults, and doesn't depend on the API.
 *
 *  Example:
 *
 *    <!-- meta:start --> defaults <!-- meta:end -->
 *
 *  Becomes:
 *
 *    {{if regexMatch "^/[0-9A-Za-z]{12}$" .OriginalReq.URL.Path}}
 *    {{httpInclude (printf "/meta%s" .OriginalReq.URL.Path)}}
 *    {{else}} defaults {{end}}
 */
function link_preview_template(): Plugin {
  return {
    name: "catalan-link-preview-template",
    apply: "build",
    transformIndexHtml(html) {
      return html.replace(
        /<!-- meta:start -->([\s\S]*?)<!-- meta:end -->/,
        (_, defaults: string) =>
          '{{if regexMatch "^/[0-9A-Za-z]{12}$" .OriginalReq.URL.Path}}' +
          '{{httpInclude (printf "/meta%s" .OriginalReq.URL.Path)}}' +
          `{{else}}${defaults}{{end}}`,
      );
    },
  };
}

export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, ".", ["VITE_", "CATALAN_"]);
  // the local catalan server, 18080 unless the dev container already has it
  const api = env.CATALAN_API_TARGET ?? "http://localhost:18080";
  return {
    plugins: [svelte(), typekit(env.VITE_TYPEKIT_ID), link_preview_template()],
    server: {
      // examples/ in the repository root holds the editor's example post
      fs: { allow: [".", "../examples"] },
      // the same paths the proxy forwards in production, to a local server
      proxy: {
        "/v1": api,
        "/meta": api,
      },
    },
    build: {
      // the render chunk is KaTeX and markdown-it, loaded apart from the shell
      chunkSizeWarningLimit: 700,
    },
    test: {
      css: false,
    },
  };
});
