/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

/****************************************************************************
 * Title
 *
 * Inline $...$ LaTeX in post titles. The pattern matches texmath's, so a
 * title parses the same way as a post body. Kept apart from render.ts, so
 * pages can check a title without loading KaTeX.
 *
 *  Example:
 *
 *    plain_title("$n^3 - n$ Proof");
 *
 *  Becomes:
 *
 *    "n^3 - n Proof"
 *
 ****************************************************************************/

// no space inside either delimiter, and no digit or backslash outside them,
// so "$5 and $10" stays text
export const inline_math = /(?<![\\\d])\$((?:[^\s\\$])|(?:\S[^$]*?[^\s\\]))\$(?!\d)/g;

/**
 * @brief True when a title has inline math to render
 *
 *  Examples:
 *
 *    has_math("$n^3 - n$ Proof");  // true
 *    has_math("$5 and $10");       // false
 */
export function has_math(title: string): boolean {
  return title.search(inline_math) !== -1;
}

/**
 * @brief A title with its math delimiters removed, for the browser tab
 */
export function plain_title(title: string): string {
  return title.replace(inline_math, "$1");
}
