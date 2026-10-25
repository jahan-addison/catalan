import { describe, expect, it } from "vitest";
import { has_math, plain_title } from "./title";

describe("has_math", () => {
  it("finds inline math the way texmath does", () => {
    expect(has_math("$n^3 - n$ Proof")).toBe(true);
    expect(has_math("Proof of $x$")).toBe(true);
    expect(has_math("n^3 - n Proof")).toBe(false);
    expect(has_math("$5 and $10")).toBe(false);
    expect(has_math("$ x $")).toBe(false);
    expect(has_math("\\$x$")).toBe(false);
  });
});

describe("plain_title", () => {
  it("removes the delimiters", () => {
    expect(plain_title("$n^3 - n$ Proof")).toBe("n^3 - n Proof");
    expect(plain_title("$5 and $10")).toBe("$5 and $10");
  });
});
