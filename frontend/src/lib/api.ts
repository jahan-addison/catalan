/*****************************************************************************
 * Copyright (c) 2026 Jahan Addison
 * License: Apache 2.0
 *
 * See the LICENSE file in the project root for the full text.
 ****************************************************************************/

/****************************************************************************
 * API client
 *
 * Typed calls to the endpoints in openapi.yaml. Requests go to /v1 on the
 * site's own origin, which the proxy forwards to the API, so the browser
 * never makes a cross-origin request. In development, Vite's proxy does the
 * same.
 ****************************************************************************/

export interface Post {
  id: string;
  title: string | null;
  author: string | null;
  body: string;
  unlisted: boolean;
  parent_id: string | null;
  created_at: string;
}

export interface Post_Summary {
  id: string;
  title: string | null;
  author: string | null;
  parent_id: string | null;
  excerpt: string;
  created_at: string;
}

export interface Post_List {
  posts: Post_Summary[];
  next_cursor: string | null;
}

export interface New_Post {
  body: string;
  title?: string;
  author?: string;
  unlisted?: boolean;
  parent_id?: string;
}

export interface Field_Error {
  field: string;
  message: string;
}

export interface Problem {
  type: string;
  title: string;
  status: number;
  detail?: string;
  errors?: Field_Error[];
}

/**
 * @brief A failed request, with the Problem Details the API sent if any
 */
export class Api_Error extends Error {
  constructor(
    readonly status: number,
    readonly problem: Problem | null,
  ) {
    super(problem?.detail ?? problem?.title ?? `request failed with ${status}`);
  }
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(path, init);
  if (!response.ok) {
    const type = response.headers.get("Content-Type") ?? "";
    const problem = type.includes("json")
      ? ((await response.json()) as Problem)
      : null;
    throw new Api_Error(response.status, problem);
  }
  return (await response.json()) as T;
}

/**
 * @brief POST /v1/posts, retried safely under the same Idempotency-Key
 */
export function create_post(post: New_Post, idempotency_key: string): Promise<Post> {
  return request<Post>("/v1/posts", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      "Idempotency-Key": `"${idempotency_key}"`,
    },
    body: JSON.stringify(post),
  });
}

/**
 * @brief GET /v1/posts/{id}, null when there is no such post
 */
export async function get_post(id: string): Promise<Post | null> {
  try {
    return await request<Post>(`/v1/posts/${encodeURIComponent(id)}`);
  } catch (error) {
    if (error instanceof Api_Error && error.status === 404) return null;
    throw error;
  }
}

/**
 * @brief GET /v1/posts, one page of listed posts
 */
export function list_posts(limit: number, cursor?: string | null): Promise<Post_List> {
  const query = new URLSearchParams({ limit: String(limit) });
  if (cursor) query.set("cursor", cursor);
  return request<Post_List>(`/v1/posts?${query}`);
}

/**
 * @brief One Idempotency-Key per draft
 *
 * Publishing the same post again, after a network failure, reuses the key
 * so the server replays the original instead of making a duplicate. Any
 * change to the post gets a new key, since it's a different request.
 *
 *  Example:
 *
 *    draft.key_for(post);  // "8e03978e-..."
 *    draft.key_for(post);  // "8e03978e-...", the same post
 *    draft.key_for(other); // "1f7c...", a new key
 */
export class Draft_Key {
  #payload = "";
  #key = "";

  key_for(post: New_Post): string {
    const payload = JSON.stringify(post);
    if (payload !== this.#payload) {
      this.#payload = payload;
      this.#key = crypto.randomUUID();
    }
    return this.#key;
  }
}
