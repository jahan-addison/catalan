/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

/**
 * @brief The render pipeline, loaded once as its own chunk
 *
 * KaTeX is most of the bundle, and only pages that render a post need it.
 * Loading it separately lets the page shell appear first, with KaTeX
 * downloading alongside the post's data.
 */
export type Renderer = (source: string) => string;

let renderer: Promise<Renderer> | null = null;

export function load_renderer(): Promise<Renderer> {
  renderer ??= import("./render").then((module) => module.render);
  return renderer;
}
