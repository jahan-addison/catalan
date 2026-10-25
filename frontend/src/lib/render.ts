/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

import DOMPurify from "dompurify";
import katex from "katex";
import MarkdownIt from "markdown-it";
import texmath from "markdown-it-texmath";
import "katex/dist/katex.min.css";
import { inline_math } from "./title";

/****************************************************************************
 * Render
 *
 * Post source to safe HTML. Posts come from anyone and are shared as links,
 * so every layer refuses to produce script:
 *
 *  * markdown-it with html: false, raw HTML in a post stays text
 *
 *  * KaTeX with trust: false, \href{javascript:...} and friends don't render
 *
 *  * DOMPurify over the result, as the last word
 *
 *  Example:
 *
 *    render("Euler: $e^{i\pi} + 1 = 0$");
 *
 *  Becomes:
 *
 *    <p>Euler: <eq><span class="katex">...</span></eq></p>
 *
 ****************************************************************************/

const katex_options = {
  throwOnError: false,
  trust: false,
  output: "htmlAndMathml",
  maxExpand: 1000,
} as const;

const markdown = new MarkdownIt({
  html: false,
  linkify: true,
  typographer: true,
}).use(texmath, {
  engine: katex,
  delimiters: "dollars",
  katexOptions: katex_options,
});

const sanitize_options = {
  USE_PROFILES: { html: true, mathMl: true, svg: true },
  ADD_TAGS: ["eq", "eqn"],
};

// links in posts lead anywhere, so they carry no referrer or ranking
DOMPurify.addHook("afterSanitizeAttributes", (node) => {
  if (node.tagName === "A") {
    node.setAttribute("rel", "nofollow noopener noreferrer");
    node.setAttribute("target", "_blank");
  }
});

/**
 * @brief Render post source, Markdown with inline $...$ and display $$...$$
 *  LaTeX, to sanitized HTML
 */
export function render(source: string): string {
  return DOMPurify.sanitize(markdown.render(source), sanitize_options);
}

/**
 * @brief Render a title's inline $...$ LaTeX to sanitized HTML
 *
 * Only math, the rest stays text. Titles are shown inside links and
 * headings, where Markdown links and blocks don't belong.
 *
 *  Example:
 *
 *    render_title("$n^3 - n$ Proof");
 *
 *  Becomes:
 *
 *    <span class="katex">...</span> Proof
 */
export function render_title(source: string): string {
  let html = "";
  let last = 0;
  for (const match of source.matchAll(inline_math)) {
    html += markdown.utils.escapeHtml(source.slice(last, match.index));
    html += katex.renderToString(match[1] ?? "", katex_options);
    last = match.index + match[0].length;
  }
  html += markdown.utils.escapeHtml(source.slice(last));
  return DOMPurify.sanitize(html, sanitize_options);
}
