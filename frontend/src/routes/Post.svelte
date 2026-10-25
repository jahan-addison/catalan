<script lang="ts">
  import { onMount } from "svelte";
  import Rendered from "../components/Rendered.svelte";
  import Title from "../components/Title.svelte";
  import { get_post, type Post } from "../lib/api";
  import { contact_email, report_link } from "../lib/contact";
  import { full_time, relative_time } from "../lib/time";
  import { plain_title } from "../lib/title";
  import Missing from "./Missing.svelte";

  let { id }: { id: string } = $props();

  let post: Post | null = $state(null);
  let status: "loading" | "ready" | "missing" | "failed" = $state("loading");
  let copied = $state(false);

  onMount(async () => {
    try {
      post = await get_post(id);
      status = post ? "ready" : "missing";
      const title = post?.title ? plain_title(post.title) : "Untitled post";
      document.title = post ? `${title} · catalan` : "catalan";
    } catch {
      status = "failed";
    }
  });

  async function copy_link(): Promise<void> {
    await navigator.clipboard.writeText(`${window.location.origin}/${id}`);
    copied = true;
    setTimeout(() => (copied = false), 1600);
  }
</script>

{#if status === "loading"}
  <p class="status" aria-live="polite">Loading…</p>
{:else if status === "missing"}
  <Missing />
{:else if status === "failed"}
  <p class="status" role="alert">Couldn't reach catalan. Try reloading.</p>
{:else if post}
  <article>
    <header class="head">
      <h1 class:untitled={!post.title}>
        {#if post.title}<Title source={post.title} />{:else}Untitled post{/if}
      </h1>
      <p class="meta">
        <span>{post.author ?? "Anonymous"}</span>
        <span aria-hidden="true">·</span>
        <time datetime={post.created_at} title={full_time(post.created_at)}>
          {relative_time(post.created_at)}
        </time>
        {#if post.parent_id}
          <span aria-hidden="true">·</span>
          <span>edit of <a href="/{post.parent_id}">{post.parent_id}</a></span>
        {/if}
        {#if post.unlisted}<span class="badge">Unlisted</span>{/if}
      </p>
    </header>

    <div class="panel body">
      <Rendered source={post.body} />
    </div>

    <div class="actions">
      <button class="button primary" onclick={copy_link}>{copied ? "Copied" : "Copy link"}</button>
      <a class="button" href="/?edit={post.id}">Edit as new</a>
      <a class="button" href="/v1/posts/{post.id}/raw" target="_blank" rel="noopener">Raw</a>
      {#if contact_email}
        <a class="report" href={report_link(contact_email, post.id, window.location.origin)}>Report</a>
      {/if}
    </div>
  </article>
{/if}

<style>
  .status {
    color: var(--muted);
  }

  .head {
    margin: 8px 0 24px;
  }

  h1 {
    margin: 0 0 8px;
    font-family: var(--font-display);
    font-size: clamp(28px, 4vw, 40px);
    font-weight: 600;
    letter-spacing: -0.015em;
    line-height: 1.15;
  }

  h1.untitled {
    color: var(--muted);
  }

  .meta {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 8px;
    margin: 0;
    color: var(--muted);
    font-size: 14px;
  }

  .meta a {
    font-family: var(--font-mono);
    font-size: 13px;
  }

  .badge {
    padding: 2px 10px;
    color: var(--accent-soft);
    font-size: 12px;
    background: rgb(var(--glow) / 14%);
    border: 1px solid var(--border);
    border-radius: 999px;
  }

  .body {
    padding: clamp(20px, 4vw, 40px);
  }

  /* the article at reading width, about 70 characters a line */
  article {
    max-width: 860px;
    margin: 0 auto;
  }

  .actions {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 12px;
    margin-top: 20px;
  }

  .report {
    margin-left: auto;
    color: var(--faint);
    font-size: 13px;
  }

  .report:hover {
    color: var(--muted);
  }
</style>
