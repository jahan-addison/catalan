import { afterEach, describe, expect, it, vi } from "vitest";
import { Api_Error, Draft_Key, create_post, get_post, list_posts } from "./api";

function respond(status: number, body: unknown, type = "application/json") {
  return vi.fn().mockResolvedValue(
    new Response(JSON.stringify(body), {
      status,
      headers: { "Content-Type": type },
    }),
  );
}

afterEach(() => vi.unstubAllGlobals());

describe("Draft_Key", () => {
  it("reuses a key for the same post and makes a new one for a change", () => {
    const draft = new Draft_Key();
    const first = draft.key_for({ body: "x" });
    expect(draft.key_for({ body: "x" })).toBe(first);
    expect(draft.key_for({ body: "y" })).not.toBe(first);
  });
});

describe("create_post", () => {
  it("sends JSON with a quoted Idempotency-Key", async () => {
    const fetch = respond(201, { id: "7HcPq2mVa0Rn" });
    vi.stubGlobal("fetch", fetch);

    await create_post({ body: "x" }, "8e03978e-40d5-43e8-bc93-6894a57f9324");

    const [path, init] = fetch.mock.calls[0] as [string, RequestInit];
    const headers = init.headers as Record<string, string>;
    expect(path).toBe("/v1/posts");
    expect(headers["Content-Type"]).toBe("application/json");
    expect(headers["Idempotency-Key"]).toBe('"8e03978e-40d5-43e8-bc93-6894a57f9324"');
    expect(headers["Turnstile-Token"]).toBeUndefined();
  });

  it("sends the Turnstile token when there is one", async () => {
    const fetch = respond(201, { id: "7HcPq2mVa0Rn" });
    vi.stubGlobal("fetch", fetch);
    await create_post({ body: "x" }, "k".repeat(16), "token-1");
    const [, init] = fetch.mock.calls[0] as [string, RequestInit];
    expect((init.headers as Record<string, string>)["Turnstile-Token"]).toBe("token-1");
  });

  it("throws the Problem Details of a 422", async () => {
    vi.stubGlobal(
      "fetch",
      respond(
        422,
        {
          type: "/problems/validation",
          title: "Validation failed",
          status: 422,
          errors: [{ field: "body", message: "must not be empty" }],
        },
        "application/problem+json",
      ),
    );

    const error = await create_post({ body: "" }, "k".repeat(16)).catch((e) => e);
    expect(error).toBeInstanceOf(Api_Error);
    expect(error.status).toBe(422);
    expect(error.problem.errors[0].field).toBe("body");
  });
});

describe("get_post", () => {
  it("returns null for a 404", async () => {
    vi.stubGlobal("fetch", respond(404, { status: 404 }, "application/problem+json"));
    expect(await get_post("000000000000")).toBeNull();
  });
});

describe("list_posts", () => {
  it("passes limit and cursor", async () => {
    const fetch = respond(200, { posts: [], next_cursor: null });
    vi.stubGlobal("fetch", fetch);
    await list_posts(5, "abc");
    expect(fetch.mock.calls[0]?.[0]).toBe("/v1/posts?limit=5&cursor=abc");
  });
});
