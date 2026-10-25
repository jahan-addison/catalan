<script lang="ts">
  import { onMount } from "svelte";

  onMount(() => {
    document.title = "Not found · catalan";
  });
</script>

<section class="missing">
  <p class="code">404</p>
  <h1>No post here</h1>
  <p>The link may be mistyped, or the post was removed.</p>
  <a class="button primary" href="/">Write a new post</a>
</section>

<style>
  .missing {
    padding: 64px 0;
    text-align: center;
  }

  .code {
    margin: 0;
    color: var(--accent-soft);
    font-family: var(--font-mono);
    letter-spacing: 0.2em;
  }

  h1 {
    margin: 8px 0;
    font-family: var(--font-display);
    font-size: 32px;
    font-weight: 600;
  }

  p {
    color: var(--muted);
  }
</style>
