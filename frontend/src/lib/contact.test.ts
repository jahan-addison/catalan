import { describe, expect, it } from "vitest";
import { report_link } from "./contact";

describe("report_link", () => {
  it("fills in the post id and link", () => {
    const link = report_link("abuse@example.com", "7HcPq2mVa0Rn", "https://example.com");
    expect(link.startsWith("mailto:abuse@example.com?")).toBe(true);
    const query = new URLSearchParams(link.split("?")[1]);
    expect(query.get("subject")).toBe("Report catalan post 7HcPq2mVa0Rn");
    expect(query.get("body")).toContain("https://example.com/7HcPq2mVa0Rn");
  });

  it("encodes spaces as %20, which every mail client reads", () => {
    expect(report_link("a@example.com")).not.toContain("+");
    expect(report_link("a@example.com")).toContain("Report%20a%20catalan%20post");
  });
});
