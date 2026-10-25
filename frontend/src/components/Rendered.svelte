<script lang="ts">
  import { load_renderer, type Renderer } from "../lib/load_renderer";

  let { source }: { source: string } = $props();

  let render = $state<Renderer | null>(null);
  load_renderer().then((loaded) => (render = loaded));

  // render() sanitizes, so the HTML is safe to insert
  const html = $derived(render ? render(source) : "");
</script>

{#if render}
  <div class="prose">{@html html}</div>
{:else}
  <div class="prose pending" aria-busy="true">{source}</div>
{/if}

<style>
  .pending {
    white-space: pre-wrap;
    color: var(--muted);
    font-family: var(--font-mono);
    font-size: 14px;
  }
</style>
