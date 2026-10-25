// @vitest-environment jsdom

import { describe, expect, it } from "vitest";
import { render, render_title } from "./render";
import basel from "../../../examples/basel-problem.md?raw";
import euler from "../../../examples/euler-identity.md?raw";
import terms from "../content/terms.md?raw";

describe("render", () => {
  it("renders Markdown", () => {
    const html = render("# Title\n\n*italic* and **bold**");
    expect(html).toContain("<h1>Title</h1>");
    expect(html).toContain("<em>italic</em>");
    expect(html).toContain("<strong>bold</strong>");
  });

  it("renders inline and display math with KaTeX", () => {
    const html = render("inline $a^2$ and display\n\n$$\\int_0^1 x\\,dx$$");
    expect(html).toContain('class="katex"');
    expect(html).toContain("katex-display");
    expect(html).toContain("<math");
  });

  it("keeps underscores inside math out of Markdown emphasis", () => {
    const html = render("$a_1 + b_1$ and $c_2$");
    expect(html).not.toContain("<em>");
  });

  it("shows raw HTML as text", () => {
    const html = render("<b>bold</b>");
    expect(html).not.toContain("<b>");
    expect(html).toContain("&lt;b&gt;");
  });

  it("never produces script", () => {
    for (const source of [
      "<script>alert(1)</script>",
      '<img src=x onerror="alert(1)">',
      "[click](javascript:alert(1))",
      "$\\href{javascript:alert(1)}{click}$",
      "$\\htmlData{onclick=alert(1)}{x}$",
    ]) {
      // checked as parsed elements, the same text shown escaped is harmless
      const page = new DOMParser().parseFromString(render(source), "text/html");
      expect(page.querySelectorAll("script")).toHaveLength(0);
      for (const element of page.querySelectorAll("*")) {
        for (const attribute of element.attributes) {
          expect(attribute.name.startsWith("on")).toBe(false);
          expect(attribute.value.trim().toLowerCase()).not.toMatch(/^javascript:/);
        }
      }
    }
  });

  it("marks links nofollow and opens them in a new tab", () => {
    const html = render("[site](https://example.com)");
    expect(html).toContain('rel="nofollow noopener noreferrer"');
    expect(html).toContain('target="_blank"');
  });

  it("renders invalid LaTeX as an error, not an exception", () => {
    expect(() => render("$\\frac{1}{$")).not.toThrow();
  });

  it("renders both example posts without a LaTeX error", () => {
    for (const example of [basel, euler]) {
      const page = new DOMParser().parseFromString(render(example), "text/html");
      expect(page.querySelectorAll(".katex-error")).toHaveLength(0);
      expect(page.querySelectorAll(".katex-display").length).toBeGreaterThan(1);
    }
  });

  it("renders the terms page, with a contact wherever one is needed", () => {
    expect(terms.split("{{contact}}").length - 1).toBeGreaterThanOrEqual(3);
    const page = new DOMParser().parseFromString(render(terms), "text/html");
    expect(page.querySelectorAll(".katex-error")).toHaveLength(0);
    expect(page.querySelector("h1")?.textContent).toBe("Terms of use");
  });
});

describe("render_title", () => {
  it("renders inline math and keeps the rest as text", () => {
    const html = render_title("$n^3 - n$ Proof");
    expect(html).toContain('class="katex"');
    expect(html).toMatch(/<\/span> Proof$/);
  });

  it("doesn't render Markdown or HTML", () => {
    const html = render_title("**bold** [link](https://example.com) <b>x</b>");
    expect(html).toBe("**bold** [link](https://example.com) &lt;b&gt;x&lt;/b&gt;");
  });

  it("stays inline after a post with display math", () => {
    render("$$\\sum_{n=1}^\\infty \\frac{1}{n^2}$$");
    expect(render_title("$n^3 - n$ Proof")).not.toContain("katex-display");
  });

  it("leaves prices as text", () => {
    expect(render_title("$5 and $10")).toBe("$5 and $10");
  });
});
