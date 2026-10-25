/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

/****************************************************************************
 * Contact
 *
 * Where to report a post, from VITE_CONTACT_EMAIL at build time. Unset, the
 * site shows no report links.
 *
 *  Example:
 *
 *    VITE_CONTACT_EMAIL=abuse@example.com
 *
 *  Becomes:
 *
 *    mailto:abuse@example.com?subject=Report%20catalan%20post%20...
 *
 ****************************************************************************/

export const contact_email: string = import.meta.env.VITE_CONTACT_EMAIL?.trim() ?? "";

/**
 * @brief A mailto link for reporting, with the post's id and link filled in
 *
 * The id is what the operator passes to the admin CLI, so it's in the
 * subject. Without a post, a general report.
 */
export function report_link(email: string, id?: string, origin?: string): string {
  const subject = id ? `Report catalan post ${id}` : "Report a catalan post";
  const body = id
    ? `Post: ${origin ?? ""}/${id}\n\nReason:\n`
    : "Post link:\n\nReason:\n";
  const query = new URLSearchParams({ subject, body }).toString().replaceAll("+", "%20");
  return `mailto:${email}?${query}`;
}
