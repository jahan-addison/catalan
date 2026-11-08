import { describe, expect, it } from "vitest";
import terms from "../content/terms.md?raw";
import { fill_terms } from "./terms";

describe("fill_terms", () => {
  it("fills in the contact, or describes the operator without one", () => {
    expect(fill_terms("Ask {{contact}}.", "abuse@example.com", false)).toBe(
      "Ask [abuse@example.com](mailto:abuse@example.com).",
    );
    expect(fill_terms("Ask {{contact}}.", "", false)).toBe("Ask the site's operator.");
  });

  it("keeps the analytics sections that match the build", () => {
    const source =
      "a\n\n{{#analytics}}\nb\n{{/analytics}}\n\nc{{^analytics}}, d{{/analytics}}.\n";
    expect(fill_terms(source, "", true)).toBe("a\n\nb\n\nc.\n");
    expect(fill_terms(source, "", false)).toBe("a\n\n\nc, d.\n");
  });

  it("mentions cookies and Google Analytics only with analytics", () => {
    const with_analytics = fill_terms(terms, "", true);
    const without = fill_terms(terms, "", false);
    expect(with_analytics).toContain("Google Analytics");
    expect(with_analytics).not.toContain("sets no cookies");
    expect(without).not.toContain("Google Analytics");
    expect(without).toContain("this site sets no cookies");
  });
});
