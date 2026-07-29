import { readFile, readdir, writeFile } from "node:fs/promises";
import { dirname, extname, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const PROJECT_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const REPOSITORY_ROOT = resolve(PROJECT_ROOT, "..");
const RESULTS_ROOT = resolve(
  PROJECT_ROOT,
  "capture_specs",
  "results",
  "dashboard_camera_capture",
);
const REPOSITORY_TOKEN = "${REPOSITORY_ROOT}";
const RUN_TEMP_TOKEN = "${RUN_TEMP}";
const USER_PATH = /[A-Za-z]:[\\/]+Users[\\/]+/i;
const RUN_TEMP =
  /[A-Za-z]:[\\/]+Users[\\/]+[^\\/]+[\\/]+AppData[\\/]+Local[\\/]+Temp[\\/]+tvbperf_[^\\/"\r\n;,]+/gi;

async function filesUnder(root) {
  const entries = await readdir(root, { withFileTypes: true });
  const files = [];
  for (const entry of entries) {
    const path = resolve(root, entry.name);
    if (entry.isDirectory()) files.push(...(await filesUnder(path)));
    else files.push(path);
  }
  return files;
}

function portable(text) {
  const roots = [
    REPOSITORY_ROOT,
    REPOSITORY_ROOT.replaceAll("\\", "/"),
    REPOSITORY_ROOT.replaceAll("\\", "\\\\"),
  ];
  let result = text;
  for (const root of roots) result = result.split(root).join(REPOSITORY_TOKEN);
  return result.replace(RUN_TEMP, RUN_TEMP_TOKEN);
}

const verifyOnly = process.argv.includes("--verify");
const candidates = (await filesUnder(RESULTS_ROOT)).filter((path) =>
  [".csv", ".json"].includes(extname(path).toLowerCase()),
);
let changed = 0;
const remaining = [];
for (const path of candidates) {
  const source = await readFile(path, "utf8");
  const sanitized = portable(source);
  if (sanitized !== source) {
    changed += 1;
    if (!verifyOnly) await writeFile(path, sanitized.replace(/\r?\n/g, "\n"));
  }
  const inspected = verifyOnly ? source : sanitized;
  if (USER_PATH.test(inspected)) {
    remaining.push(relative(PROJECT_ROOT, path).replaceAll("\\", "/"));
  }
}

if (remaining.length) {
  throw new Error(
    `Capture results retain absolute user paths: ${remaining.join(", ")}`,
  );
}
if (verifyOnly && changed) {
  throw new Error(`${changed} capture result files are not portable.`);
}
console.log(
  `${verifyOnly ? "Verified" : "Sanitized"} ${candidates.length} capture result files; ` +
    `${changed} ${verifyOnly ? "would change" : "changed"}, 0 local user paths.`,
);
