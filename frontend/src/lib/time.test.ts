import { describe, expect, it } from "vitest";
import { relative_time } from "./time";

describe("relative_time", () => {
  const now = new Date("2026-10-24T03:15:00.000Z");

  it("names recent, hourly, and daily differences", () => {
    expect(relative_time("2026-10-24T03:14:40.000Z", now)).toBe("just now");
    expect(relative_time("2026-10-24T03:10:00.000Z", now)).toBe("5 minutes ago");
    expect(relative_time("2026-10-24T01:15:00.000Z", now)).toBe("2 hours ago");
    expect(relative_time("2026-10-23T03:15:00.000Z", now)).toBe("yesterday");
    expect(relative_time("2025-10-24T03:15:00.000Z", now)).toBe("last year");
  });
});
