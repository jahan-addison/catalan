/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

/****************************************************************************
 * Terms
 *
 * The terms page's source, filled in for this site's build. {{contact}} is
 * the operator's contact. An {{#analytics}} section is kept only when the
 * build has the Google Analytics tag, and an {{^analytics}} section only
 * when it doesn't. A section on its own lines takes its last newline with it.
 *
 *  Example:
 *
 *    No cookies{{^analytics}}, ever{{/analytics}}. Ask {{contact}}.
 *    {{#analytics}}
 *    This site uses Google Analytics.
 *    {{/analytics}}
 *
 *  Becomes, without analytics:
 *
 *    No cookies, ever. Ask [abuse@example.com](mailto:abuse@example.com).
 *
 ****************************************************************************/

import { contact_email } from "./contact";

// the tag is added to production builds only, see vite.config.ts
export const analytics: boolean =
  import.meta.env.PROD && Boolean(import.meta.env.VITE_GA_MEASUREMENT_ID?.trim());

// "#" or "^", an optional newline, the text, and the same newline after
const analytics_section = /\{\{([#^])analytics\}\}(\n?)([\s\S]*?)\{\{\/analytics\}\}\2/g;

/**
 * @brief Fill in the terms for a contact email, or none, and analytics
 *
 * Without an email, the contact is a description of the operator.
 */
export function fill_terms(source: string, email: string, with_analytics: boolean): string {
  const contact = email ? `[${email}](mailto:${email})` : "the site's operator";
  return source
    .replaceAll("{{contact}}", contact)
    .replace(analytics_section, (_, kind: string, _newline: string, text: string) =>
      (kind === "#") === with_analytics ? text : "",
    );
}

/**
 * @brief The terms for this site's build
 */
export function site_terms(source: string): string {
  return fill_terms(source, contact_email, analytics);
}
