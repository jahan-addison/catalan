<script lang="ts">
  import { load_renderer, type Renderer } from "../lib/load_renderer";
  import { has_math } from "../lib/title";

  let { source }: { source: string } = $props();

  let renderer = $state<Renderer | null>(null);
  $effect(() => {
    if (has_math(source)) load_renderer().then((loaded) => (renderer = loaded));
  });

  // render_title() sanitizes, so the HTML is safe to insert
  const html = $derived(renderer && has_math(source) ? renderer.render_title(source) : null);
</script>

{#if html !== null}{@html html}{:else}{source}{/if}
