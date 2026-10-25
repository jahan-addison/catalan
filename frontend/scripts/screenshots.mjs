// Screenshots of every page at desktop and phone widths, for checking the
// design by eye. Expects the dev server (npm run dev) and a local catalan
// server with at least one post. Writes to screenshots/.
//
//   npm run screenshots
//   BASE=http://localhost:18082 npm run screenshots

import { mkdir } from "node:fs/promises";
import { chromium } from "playwright";

const base = process.env.BASE ?? "http://localhost:5173";
const out = new URL("../screenshots/", import.meta.url);
await mkdir(out, { recursive: true });

const sizes = {
  desktop: { width: 1440, height: 1000 },
  phone: { width: 390, height: 844 },
};

const browser = await chromium.launch();

async function shoot(name, path, size, prepare) {
  const page = await browser.newPage({ viewport: sizes[size], deviceScaleFactor: 2 });
  await page.goto(base + path, { waitUntil: "networkidle" });
  if (prepare) await prepare(page);
  await page.waitForTimeout(300);
  await page.screenshot({ path: new URL(`${name}-${size}.png`, out).pathname, fullPage: true });
  await page.close();
}

const first = await (await fetch(`${base}/v1/posts?limit=1`)).json();
const id = first.posts[0]?.id;

for (const size of Object.keys(sizes)) {
  await shoot("editor", "/", size);
  await shoot("editor-filled", "/", size, async (page) => {
    await page.fill("#title", "The Basel problem");
    await page.fill(
      "#body",
      "## Statement\n\nEuler showed that\n\n$$\\sum_{n=1}^{\\infty} \\frac{1}{n^2} = \\frac{\\pi^2}{6}$$\n\n" +
        "where the partial sums $S_N = \\sum_{n=1}^{N} n^{-2}$ converge *slowly*.",
    );
    if (size === "phone") await page.click("text=Preview");
  });
  if (id) await shoot("post", `/${id}`, size);
  await shoot("all", "/all", size);
  await shoot("missing", "/does-not-exist", size);
}

await browser.close();
console.log(`screenshots in ${out.pathname}`);
