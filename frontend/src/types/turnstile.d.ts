interface Turnstile_Options {
  sitekey: string;
  theme?: "light" | "dark" | "auto";
  appearance?: "always" | "execute" | "interaction-only";
  callback?: (token: string) => void;
  "expired-callback"?: () => void;
  "error-callback"?: () => void;
}

interface Turnstile {
  render(container: HTMLElement, options: Turnstile_Options): string;
  reset(widget?: string): void;
  remove(widget?: string): void;
}

interface Window {
  turnstile?: Turnstile;
}
