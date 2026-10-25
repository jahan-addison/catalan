<script lang="ts">
  import { contact_email, report_link } from "./lib/contact";
  import { intercept_links, location_state, match } from "./lib/router.svelte";
  import All from "./routes/All.svelte";
  import Editor from "./routes/Editor.svelte";
  import Missing from "./routes/Missing.svelte";
  import Post from "./routes/Post.svelte";
  import Terms from "./routes/Terms.svelte";

  const page = $derived(match(location_state.path, location_state.search));
</script>

<svelte:document onclick={intercept_links} />

<div class="shell">
  <header class="masthead">
    <a class="brand" href="/" aria-label="catalan, new post">
      <span class="mark" aria-hidden="true"><span>C<sub>n</sub></span></span>
      <span class="name">catalan</span>
    </a>
    <nav class="nav">
      <a href="/" aria-current={page.name === "editor" ? "page" : undefined}>New</a>
      <a href="/all" aria-current={page.name === "all" ? "page" : undefined}>All posts</a>
    </nav>
  </header>

  <main>
    {#if page.name === "editor"}
      {#key page.edit}
        <Editor edit={page.edit} />
      {/key}
    {:else if page.name === "all"}
      <All />
    {:else if page.name === "terms"}
      <Terms />
    {:else if page.name === "post"}
      {#key page.id}
        <Post id={page.id} />
      {/key}
    {:else}
      <Missing />
    {/if}
  </main>

  <footer class="footer">
    <span>A pastebin for math and LaTeX.</span>
    <span class="links">
      <a href="/terms">Terms</a>
      {#if contact_email}<a href={report_link(contact_email)}>Report a post</a>{/if}
      <a href="https://github.com/jahan-addison/catalan">Source</a>
    </span>
  </footer>
</div>

<style>
  .shell {
    display: flex;
    flex-direction: column;
    min-height: 100vh;
    max-width: var(--page);
    margin: 0 auto;
    padding: 0 24px;
  }

  .masthead {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 28px 0 20px;
  }

  .brand {
    display: inline-flex;
    align-items: center;
    gap: 12px;
    color: var(--text);
    text-decoration: none;
  }

  .mark {
    display: inline-grid;
    place-items: center;
    width: 38px;
    height: 38px;
    font-family: var(--font-post);
    font-size: 20px;
    font-style: italic;
    color: #f5ecff;
    background: linear-gradient(145deg, #c084fc, #7e22ce);
    border-radius: 11px;
    box-shadow: 0 6px 22px -6px rgb(var(--glow) / 80%);
  }

  .mark > span {
    line-height: 1;
    transform: translateX(-1px);
  }

  .mark sub {
    font-size: 11px;
    vertical-align: -0.3em;
  }

  .name {
    font-family: var(--font-display);
    font-size: 24px;
    font-weight: 600;
    letter-spacing: -0.01em;
  }

  .nav {
    display: flex;
    gap: 4px;
    padding: 4px;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 999px;
  }

  .nav a {
    padding: 6px 14px;
    color: var(--muted);
    font-size: 14px;
    text-decoration: none;
    border-radius: 999px;
  }

  .nav a:hover {
    color: var(--text);
  }

  .nav a[aria-current="page"] {
    color: var(--text);
    background: var(--surface-strong);
  }

  main {
    flex: 1;
  }

  .footer {
    display: flex;
    justify-content: space-between;
    gap: 16px;
    padding: 48px 0 32px;
    color: var(--faint);
    font-size: 13px;
  }

  .footer a {
    color: var(--muted);
  }

  .links {
    display: flex;
    gap: 20px;
  }
</style>
