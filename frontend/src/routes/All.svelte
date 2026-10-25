<script lang="ts">
  import { onMount } from "svelte";
  import Post_List from "../components/Post_List.svelte";
  import { list_posts, type Post_Summary } from "../lib/api";

  let posts: Post_Summary[] = $state([]);
  let next: string | null = $state(null);
  let status: "loading" | "ready" | "failed" = $state("loading");
  let loading_more = $state(false);

  onMount(async () => {
    document.title = "All posts · catalan";
    try {
      const page = await list_posts(20);
      posts = page.posts;
      next = page.next_cursor;
      status = "ready";
    } catch {
      status = "failed";
    }
  });

  async function load_more(): Promise<void> {
    if (!next || loading_more) return;
    loading_more = true;
    try {
      const page = await list_posts(20, next);
      posts = [...posts, ...page.posts];
      next = page.next_cursor;
    } finally {
      loading_more = false;
    }
  }
</script>

<h1>All posts</h1>
<p class="lede">Newest first. Unlisted posts never appear here.</p>

{#if status === "loading"}
  <p class="status" aria-live="polite">Loading…</p>
{:else if status === "failed"}
  <p class="status" role="alert">Couldn't reach catalan. Try reloading.</p>
{:else if posts.length === 0}
  <p class="status">No posts yet. <a href="/">Write the first one.</a></p>
{:else}
  <div class="panel list">
    <Post_List {posts} />
  </div>
  {#if next}
    <div class="more">
      <button class="button" onclick={load_more} disabled={loading_more}>
        {loading_more ? "Loading…" : "Load more"}
      </button>
    </div>
  {/if}
{/if}

<style>
  h1 {
    margin: 8px 0 6px;
    font-family: var(--font-display);
    font-size: 32px;
    font-weight: 600;
    letter-spacing: -0.015em;
  }

  .lede,
  .status {
    color: var(--muted);
  }

  .lede {
    margin: 0 0 24px;
  }

  .list {
    padding: 8px;
  }

  .more {
    display: flex;
    justify-content: center;
    margin-top: 20px;
  }
</style>
