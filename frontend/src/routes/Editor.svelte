<script lang="ts">
  import { onMount } from "svelte";
  import Human_Check from "../components/Human_Check.svelte";
  import Post_List from "../components/Post_List.svelte";
  import Rendered from "../components/Rendered.svelte";
  import {
    Api_Error,
    Draft_Key,
    create_post,
    get_post,
    list_posts,
    type New_Post,
    type Post,
    type Post_Summary,
  } from "../lib/api";
  import { navigate } from "../lib/router.svelte";
  // the BaselProblem seed post's body, the same file the server embeds
  import example from "../../../examples/basel-problem.md?raw";

  let { edit }: { edit: string | null } = $props();

  const author_key = "catalan:author";
  // the human check, off when the site has no Turnstile site key
  const site_key = import.meta.env.VITE_TURNSTILE_SITE_KEY?.trim() ?? "";
  const body_limit = 65536;
  let title = $state("");
  let author = $state(read_author());
  // App remounts the editor when edit changes, so its first value is the one
  // svelte-ignore state_referenced_locally
  let body = $state(edit ? "" : example);
  let unlisted = $state(false);
  let parent: Post | null = $state(null);
  let missing_parent = $state(false);

  let field_errors: Record<string, string> = $state({});
  let error = $state("");
  let publishing = $state(false);
  let tab: "write" | "preview" = $state("write");
  let recent: Post_Summary[] = $state([]);
  let human_token: string | null = $state(null);
  let human_check: Human_Check | null = $state(null);

  const draft = new Draft_Key();
  const length = $derived([...body].length);
  // the untouched example can't be published, it already is at /BaselProblem
  const untouched = $derived(!parent && body === example);

  function read_author(): string {
    try {
      return localStorage.getItem(author_key) ?? "";
    } catch {
      return "";
    }
  }

  function remember_author(): void {
    try {
      if (author.trim()) localStorage.setItem(author_key, author.trim());
      else localStorage.removeItem(author_key);
    } catch {
      // storage blocked, the author just isn't remembered
    }
  }

  onMount(async () => {
    document.title = "catalan";
    list_posts(5)
      .then((page) => (recent = page.posts))
      .catch(() => (recent = []));

    if (edit) {
      const post = await get_post(edit).catch(() => null);
      if (!post) {
        missing_parent = true;
        return;
      }
      parent = post;
      title = post.title ?? "";
      body = post.body;
      unlisted = post.unlisted;
    }
  });

  function new_post(): New_Post {
    const post: New_Post = { body };
    if (title.trim()) post.title = title.trim();
    if (author.trim()) post.author = author.trim();
    if (unlisted) post.unlisted = true;
    if (parent) post.parent_id = parent.id;
    return post;
  }

  async function publish(): Promise<void> {
    if (publishing || untouched) return;
    field_errors = {};
    error = "";

    if (site_key && !human_token) {
      error = "Still checking that you're human, publish again in a moment.";
      return;
    }

    const post = new_post();
    publishing = true;
    try {
      const created = await create_post(post, draft.key_for(post), human_token);
      remember_author();
      navigate(`/${created.id}`);
    } catch (failure) {
      if (failure instanceof Api_Error && failure.status === 422 && failure.problem?.errors) {
        // validation runs before the human check, so the token is unspent
        field_errors = Object.fromEntries(
          failure.problem.errors.map((e) => [e.field, e.message]),
        );
        if (field_errors.body) tab = "write";
      } else {
        // the token may be spent, tokens are single use
        human_check?.reset();
        error =
          failure instanceof Api_Error
            ? failure.message
            : "Couldn't reach catalan. Publishing again is safe, it won't make a duplicate.";
      }
    } finally {
      publishing = false;
    }
  }

  function clear_example(): void {
    body = "";
    document.getElementById("body")?.focus();
  }

  function shortcut(event: KeyboardEvent): void {
    if (event.key === "Enter" && (event.metaKey || event.ctrlKey)) {
      event.preventDefault();
      void publish();
    }
  }
</script>

<svelte:window onkeydown={shortcut} />

<section class="compose">
  <p class="lede">Write Markdown with LaTeX, then share the link.</p>

  {#if missing_parent}
    <p class="notice">That post doesn't exist, so this is a new post instead of an edit.</p>
  {:else if parent}
    <p class="notice">
      Editing <a href="/{parent.id}">{parent.title ?? "an untitled post"}</a> as a new post. The
      original stays unchanged.
    </p>
  {/if}

  <div class="details">
    <div class="title-field">
      <label class="field-label" for="title">Title</label>
      <input id="title" class="input title" bind:value={title} maxlength="200" placeholder="Your problem name" />
      {#if field_errors.title}<p class="field-error">{field_errors.title}</p>{/if}
    </div>
    <div>
      <label class="field-label" for="author">Author</label>
      <input id="author" class="input" bind:value={author} maxlength="64" placeholder="Your name" />
      {#if field_errors.author}<p class="field-error">{field_errors.author}</p>{/if}
    </div>
  </div>

  <div class="tabs" role="tablist" aria-label="Editor">
    <button role="tab" aria-selected={tab === "write"} onclick={() => (tab = "write")}>Write</button>
    <button role="tab" aria-selected={tab === "preview"} onclick={() => (tab = "preview")}>Preview</button>
  </div>

  <div class="workspace" data-tab={tab}>
    <div class="pane write">
      <div class="pane-head">
        <label class="field-label" for="body">Post</label>
        {#if untouched}
          <span class="example-bar">
            <span class="badge">Example</span>
            <a href="/BaselProblem">See it published</a>
            <button class="link-button" onclick={clear_example}>Clear example</button>
          </span>
        {:else if !parent && !body}
          <button class="link-button" onclick={() => (body = example)}>Show the example</button>
        {/if}
      </div>
      <textarea
        id="body"
        class="editor-input"
        bind:value={body}
        placeholder="Markdown, with $inline$ and $$display$$ LaTeX"
        spellcheck="false"
        aria-invalid={field_errors.body ? "true" : undefined}
      ></textarea>
      <div class="under">
        {#if field_errors.body}<p class="field-error">{field_errors.body}</p>{:else}<span></span>{/if}
        {#if length > body_limit * 0.9}
          <span class="count" class:over={length > body_limit}>{length} / {body_limit}</span>
        {/if}
      </div>
    </div>

    <div class="pane preview">
      <div class="pane-head"><span class="field-label">Preview</span></div>
      <div class="panel rendered">
        {#if body.trim()}
          <Rendered source={body} />
        {:else}
          <p class="empty">Your post appears here as you write.</p>
        {/if}
      </div>
      <div class="under"></div>
    </div>
  </div>

  {#if field_errors.parent_id}<p class="field-error">{field_errors.parent_id}</p>{/if}

  <div class="actions">
    <button class="button primary" onclick={publish} disabled={publishing || untouched}>
      {publishing ? "Publishing…" : "Publish"}
    </button>
    <label class="toggle">
      <input type="checkbox" bind:checked={unlisted} />
      <span>Unlisted</span>
    </label>
    <span class="consent">By publishing, you agree to the <a href="/terms">terms</a>.</span>
    <span class="hint" class:explains={untouched}>
      {untouched ? "Edit or clear the example to publish" : "⌘ Enter to publish"}
    </span>
  </div>

  {#if site_key}
    <Human_Check bind:this={human_check} {site_key} on_token={(token) => (human_token = token)} />
  {/if}

  {#if error}<p class="field-error" role="alert">{error}</p>{/if}
</section>

{#if recent.length}
  <section class="recent panel" aria-labelledby="recent-heading">
    <div class="recent-head">
      <h2 id="recent-heading">Recent</h2>
      <a href="/all">See all</a>
    </div>
    <Post_List posts={recent} />
  </section>
{/if}

<style>
  .compose {
    padding-top: 8px;
  }

  .lede {
    margin: 0 0 28px;
    color: var(--muted);
    font-size: 18px;
  }

  .notice {
    margin: 0 0 20px;
    padding: 12px 16px;
    color: var(--muted);
    background: rgb(var(--glow) / 10%);
    border: 1px solid var(--border);
    border-radius: 10px;
  }

  .details {
    display: grid;
    grid-template-columns: 1fr 260px;
    gap: 16px;
    margin-bottom: 20px;
  }

  .details .input {
    height: 52px;
  }

  .input.title {
    font-family: var(--font-display);
    font-size: 20px;
  }

  .tabs {
    display: none;
  }

  .workspace {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 24px;
  }

  .pane {
    display: flex;
    flex-direction: column;
    min-width: 0;
  }

  .editor-input {
    flex: 1;
    min-height: 460px;
    padding: 16px;
    font-family: var(--font-mono);
    font-size: 14px;
    line-height: 1.65;
    resize: vertical;
    tab-size: 2;
  }

  .rendered {
    flex: 1;
    min-height: 460px;
    padding: 20px 24px;
    overflow: auto;
  }

  .pane-head {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    gap: 12px;
    min-height: 26px;
  }

  .example-bar {
    display: inline-flex;
    align-items: baseline;
    gap: 12px;
    font-size: 13px;
  }

  .badge {
    padding: 1px 9px;
    color: var(--accent-soft);
    font-size: 12px;
    background: rgb(var(--glow) / 14%);
    border: 1px solid var(--border);
    border-radius: 999px;
  }

  .link-button {
    padding: 0;
    color: var(--accent-soft);
    font: inherit;
    font-size: 13px;
    background: none;
    border: 0;
    text-decoration: underline;
    text-decoration-color: rgb(192 132 252 / 40%);
    text-underline-offset: 3px;
    cursor: pointer;
  }

  .link-button:hover {
    text-decoration-color: currentColor;
  }

  .empty {
    margin: 0;
    color: var(--faint);
  }

  .under {
    display: flex;
    justify-content: space-between;
    min-height: 22px;
  }

  .count {
    margin-top: 6px;
    color: var(--faint);
    font-size: 12px;
    font-family: var(--font-mono);
  }

  .count.over {
    color: var(--danger);
  }

  .actions {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 12px 20px;
    margin-top: 20px;
  }

  .toggle {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    color: var(--muted);
    cursor: pointer;
  }

  .toggle input {
    width: 16px;
    height: 16px;
    accent-color: var(--accent);
  }

  .consent {
    color: var(--faint);
    font-size: 13px;
  }

  .hint {
    margin-left: auto;
    color: var(--faint);
    font-size: 13px;
  }

  .recent {
    margin-top: 48px;
    padding: 18px 10px 10px;
  }

  .recent-head {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    padding: 0 14px 6px;
  }

  .recent-head h2 {
    margin: 0;
    font-family: var(--font-display);
    font-size: 16px;
    font-weight: 600;
  }

  .recent-head a {
    font-size: 14px;
  }

  @media (max-width: 860px) {
    .details {
      grid-template-columns: 1fr;
    }

    .tabs {
      display: flex;
      gap: 4px;
      margin-bottom: 12px;
      padding: 4px;
      width: fit-content;
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 999px;
    }

    .tabs button {
      padding: 6px 16px;
      color: var(--muted);
      font: inherit;
      font-size: 14px;
      background: none;
      border: 0;
      border-radius: 999px;
      cursor: pointer;
    }

    .tabs button[aria-selected="true"] {
      color: var(--text);
      background: var(--surface-strong);
    }

    .workspace {
      grid-template-columns: 1fr;
    }

    .workspace[data-tab="write"] .preview,
    .workspace[data-tab="preview"] .write {
      display: none;
    }

    .editor-input,
    .rendered {
      min-height: 340px;
    }

    .hint:not(.explains) {
      display: none;
    }
  }
</style>
