/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

/****************************************************************************
 * Router
 *
 * Three pages, matched on the path. The proxy serves index.html for every
 * path, so a shared link opens straight to its page.
 *
 *  Example:
 *
 *    /                 new post
 *    /?edit=<id>       new post, an edit of <id>
 *    /all              every listed post
 *    /7HcPq2mVa0Rn     a post
 *
 ****************************************************************************/

export type Page =
  | { name: "editor"; edit: string | null }
  | { name: "all" }
  | { name: "post"; id: string }
  | { name: "missing" };

const post_path = /^\/([0-9A-Za-z]{12})$/;

/**
 * @brief The page for a path and query string
 */
export function match(path: string, search: string): Page {
  if (path === "/") return { name: "editor", edit: new URLSearchParams(search).get("edit") };
  if (path === "/all") return { name: "all" };
  const post = post_path.exec(path);
  if (post?.[1]) return { name: "post", id: post[1] };
  return { name: "missing" };
}

export const location_state = $state({
  path: window.location.pathname,
  search: window.location.search,
});

function update(): void {
  location_state.path = window.location.pathname;
  location_state.search = window.location.search;
}

/**
 * @brief Go to a page without a reload
 */
export function navigate(to: string): void {
  window.history.pushState({}, "", to);
  update();
  window.scrollTo(0, 0);
}

/**
 * @brief Follow same-origin links with navigate, everything else as usual
 *
 * Modified clicks (new tab, new window), other origins, and links with a
 * target keep the browser's behavior.
 */
export function intercept_links(event: MouseEvent): void {
  if (event.defaultPrevented || event.button !== 0) return;
  if (event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return;
  const anchor = (event.target as Element | null)?.closest("a");
  if (!anchor || anchor.target || anchor.hasAttribute("download")) return;
  const url = new URL(anchor.href);
  if (url.origin !== window.location.origin) return;
  if (url.pathname.startsWith("/v1/")) return;
  event.preventDefault();
  navigate(url.pathname + url.search);
}

window.addEventListener("popstate", update);
