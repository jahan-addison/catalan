<script lang="ts">
  import { onMount } from "svelte";
  import Rendered from "../components/Rendered.svelte";
  import terms from "../content/terms.md?raw";
  import { contact_email } from "../lib/contact";

  // the operator's contact, or a description when the site has none
  const contact = contact_email
    ? `[${contact_email}](mailto:${contact_email})`
    : "the site's operator";
  const source = terms.replaceAll("{{contact}}", contact);

  // the title, then the short version in a card, then the sections
  const [preamble = "", summary = "", ...sections] = source.split(/\n(?=## )/);
  const rest = sections.join("\n");

  onMount(() => {
    document.title = "Terms of use · catalan";
  });
</script>

<article class="terms">
  <Rendered source={preamble} />
  <div class="panel summary">
    <Rendered source={summary} />
  </div>
  <Rendered source={rest} />
</article>

<style>
  .terms {
    max-width: 760px;
    margin: 8px auto 0;
  }

  .terms :global(.prose) {
    font-size: 1.1rem;
  }

  .terms :global(.prose h1) {
    margin-bottom: 0.25em;
    font-family: var(--font-display);
    font-size: clamp(2rem, 4vw, 2.6rem);
    font-weight: 600;
    letter-spacing: -0.015em;
  }

  .terms :global(.prose > p:first-of-type em) {
    color: var(--muted);
    font-size: 0.9em;
  }

  /* each section starts with a heading well clear of the text above */
  .terms :global(.prose h2) {
    margin: 2.4em 0 0.6em;
    font-family: var(--font-display);
    font-size: 1.6rem;
    font-weight: 600;
    letter-spacing: -0.01em;
    line-height: 1.2;
  }

  .summary {
    margin: 32px 0 8px;
    padding: 24px 28px 8px;
  }

  .summary :global(.prose h2) {
    margin: 0 0 0.8em;
    color: var(--accent-soft);
    font-family: var(--font-ui);
    font-size: 0.8rem;
    font-weight: 600;
    letter-spacing: 0.08em;
    text-transform: uppercase;
  }

  .summary :global(.prose li) {
    margin-bottom: 0.6em;
  }
</style>
