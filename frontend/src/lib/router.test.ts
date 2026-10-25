// @vitest-environment jsdom

import { describe, expect, it } from "vitest";
import { match } from "./router.svelte";

describe("match", () => {
  it("maps paths to pages", () => {
    expect(match("/", "")).toEqual({ name: "editor", edit: null });
    expect(match("/", "?edit=7HcPq2mVa0Rn")).toEqual({ name: "editor", edit: "7HcPq2mVa0Rn" });
    expect(match("/all", "")).toEqual({ name: "all" });
    expect(match("/terms", "")).toEqual({ name: "terms" });
    expect(match("/7HcPq2mVa0Rn", "")).toEqual({ name: "post", id: "7HcPq2mVa0Rn" });
    expect(match("/7HcPq2mVa0R", "")).toEqual({ name: "missing" });
    expect(match("/nope/deeper", "")).toEqual({ name: "missing" });
  });
});
