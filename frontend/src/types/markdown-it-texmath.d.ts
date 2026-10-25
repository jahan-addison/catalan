declare module "markdown-it-texmath" {
  import type { PluginWithOptions } from "markdown-it";

  interface Texmath_Options {
    engine: unknown;
    delimiters?: string | string[];
    katexOptions?: Record<string, unknown>;
  }

  const texmath: PluginWithOptions<Texmath_Options>;
  export default texmath;
}
