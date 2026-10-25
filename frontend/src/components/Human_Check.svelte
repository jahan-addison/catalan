<script lang="ts">
  import { onDestroy, onMount } from "svelte";

  let {
    site_key,
    on_token,
  }: { site_key: string; on_token: (token: string | null) => void } = $props();

  let container: HTMLElement;
  let widget: string | null = null;

  const script_url = "https://challenges.cloudflare.com/turnstile/v0/api.js?render=explicit";

  // one script tag for the page, however many times the editor mounts
  function load_turnstile(): Promise<Turnstile> {
    if (window.turnstile) return Promise.resolve(window.turnstile);
    let script = document.querySelector<HTMLScriptElement>(`script[src="${script_url}"]`);
    if (!script) {
      script = document.createElement("script");
      script.src = script_url;
      script.async = true;
      document.head.append(script);
    }
    return new Promise((resolve, reject) => {
      script.addEventListener("load", () => resolve(window.turnstile!));
      script.addEventListener("error", () => reject(new Error("turnstile didn't load")));
    });
  }

  onMount(async () => {
    try {
      const turnstile = await load_turnstile();
      widget = turnstile.render(container, {
        sitekey: site_key,
        theme: "dark",
        appearance: "interaction-only",
        callback: (token) => on_token(token),
        "expired-callback": () => on_token(null),
        "error-callback": () => on_token(null),
      });
    } catch {
      on_token(null);
    }
  });

  onDestroy(() => {
    if (widget) window.turnstile?.remove(widget);
  });

  /**
   * @brief Reset the widget for a fresh token
   *
   * Tokens are single use, so a failed attempt needs a new one.
   */
  export function reset(): void {
    on_token(null);
    if (widget) window.turnstile?.reset(widget);
  }
</script>

<div class="human-check" bind:this={container}></div>

<style>
  .human-check:empty {
    display: none;
  }
</style>
