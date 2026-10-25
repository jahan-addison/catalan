/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

const units: [Intl.RelativeTimeFormatUnit, number][] = [
  ["year", 365 * 24 * 60 * 60],
  ["month", 30 * 24 * 60 * 60],
  ["week", 7 * 24 * 60 * 60],
  ["day", 24 * 60 * 60],
  ["hour", 60 * 60],
  ["minute", 60],
];

const relative = new Intl.RelativeTimeFormat("en", { numeric: "auto" });

/**
 * @brief How long ago an ISO 8601 time was, for people
 *
 *  Examples:
 *
 *    relative_time("2026-10-24T03:15:00.000Z", now);  // "just now"
 *    relative_time("2026-10-24T01:15:00.000Z", now);  // "2 hours ago"
 *    relative_time("2026-10-23T03:15:00.000Z", now);  // "yesterday"
 */
export function relative_time(iso: string, now: Date = new Date()): string {
  const seconds = Math.round((new Date(iso).getTime() - now.getTime()) / 1000);
  for (const [unit, size] of units) {
    if (Math.abs(seconds) >= size) return relative.format(Math.round(seconds / size), unit);
  }
  return "just now";
}

/**
 * @brief An ISO 8601 time as a full local date and time, for tooltips
 */
export function full_time(iso: string): string {
  return new Date(iso).toLocaleString("en", { dateStyle: "long", timeStyle: "short" });
}
