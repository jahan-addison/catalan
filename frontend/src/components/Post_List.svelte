<script lang="ts">
  import type { Post_Summary } from "../lib/api";
  import { full_time, relative_time } from "../lib/time";

  let { posts }: { posts: Post_Summary[] } = $props();
</script>

<ul class="posts">
  {#each posts as post (post.id)}
    <li>
      <a class="post" href="/{post.id}">
        <span class="title" class:untitled={!post.title}>
          {post.title ?? post.excerpt}
        </span>
        <span class="meta">
          {#if post.author}<span>{post.author}</span><span aria-hidden="true">·</span>{/if}
          <time datetime={post.created_at} title={full_time(post.created_at)}>
            {relative_time(post.created_at)}
          </time>
          {#if post.parent_id}<span aria-hidden="true">·</span><span>edit</span>{/if}
        </span>
      </a>
    </li>
  {/each}
</ul>

<style>
  .posts {
    margin: 0;
    padding: 0;
    list-style: none;
  }

  .post {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    gap: 16px;
    padding: 12px 14px;
    color: var(--text);
    text-decoration: none;
    border-radius: 10px;
  }

  .post:hover {
    background: var(--surface-strong);
  }

  .title {
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }

  .title.untitled {
    color: var(--muted);
    font-family: var(--font-mono);
    font-size: 13px;
  }

  .meta {
    display: flex;
    flex-shrink: 0;
    gap: 6px;
    color: var(--faint);
    font-size: 13px;
  }
</style>
