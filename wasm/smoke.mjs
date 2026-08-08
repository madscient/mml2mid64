// Smoke test for the WebAssembly build.
//
//   emcmake cmake -S . -B build-wasm
//   cmake --build build-wasm
//   node wasm/smoke.mjs build-wasm/mml2mid.mjs
//
// Checks that the module compiles an MML, writes both the SMF and the debug
// map, and reports errors through --diag=json.  It also compiles the same MML
// twice with two separate instances, which is how the VS Code extension is
// meant to use it (one instance per compilation, then thrown away -- see the
// EMSCRIPTEN block in CMakeLists.txt for why).

import { readFileSync } from "node:fs";
import { pathToFileURL } from "node:url";

const modPath = process.argv[2] ?? "build-wasm/mml2mid.mjs";
const { default: createMml2mid } = await import(pathToFileURL(modPath).href);

const SRC = ["A C1 o4 l4 cdef", "A gab>c", ""].join("\n");

// Run one compilation in a fresh instance.  Returns stdout/stderr and whatever
// the run left in the in-memory filesystem.
//
// Emscripten's Node shell assigns process.exitCode when the program exits, so a
// deliberately failing compilation would otherwise decide this script's exit
// status.  Save and restore it around the call.
async function compile(args, files) {
  const out = [];
  const err = [];
  const savedExitCode = process.exitCode;
  const mod = await createMml2mid({
    noInitialRun: true,
    print: (s) => out.push(s),
    printErr: (s) => err.push(s),
  });

  for (const [name, data] of Object.entries(files)) {
    mod.FS.writeFile(name, data);
  }

  let status = 0;
  try {
    status = mod.callMain(args) ?? 0;
  } catch (e) {
    // Emscripten throws ExitStatus when main() calls exit() with EXIT_RUNTIME.
    if (e && typeof e.status === "number") status = e.status;
    else throw e;
  }

  const read = (name) => {
    try {
      return mod.FS.readFile(name);
    } catch {
      return null;
    }
  };
  process.exitCode = savedExitCode;
  return { status, out: out.join("\n"), err: err.join("\n"), read };
}

let failures = 0;

function check(label, ok, detail = "") {
  console.log(`${ok ? "ok  " : "FAIL"}  ${label}${detail ? "  -- " + detail : ""}`);
  if (!ok) failures++;
}

// --- 1. a normal compilation with -g2 -----------------------------------
{
  const r = await compile(["-g2", "song.mml", "song.mid"], { "song.mml": SRC });
  check("compiles without error", r.status === 0, `status=${r.status} ${r.err}`);

  const mid = r.read("song.mid");
  check("wrote an SMF", mid !== null && mid.length > 0);
  check(
    "SMF starts with MThd",
    mid !== null && Buffer.from(mid.subarray(0, 4)).toString() === "MThd",
  );

  const rawMap = r.read("song.mmlmap.json");
  check("wrote a debug map", rawMap !== null);
  if (rawMap) {
    const map = JSON.parse(Buffer.from(rawMap).toString("utf8"));
    check("map version is 1", map.version === 1, JSON.stringify(map.version));
    check("map level is 2", map.level === 2);
    check("map names the source", map.files?.[0] === "song.mml");
    // The two source lines are 4 quarter notes each at timebase 48.
    check(
      "line table maps tick 0 and 192",
      JSON.stringify(map.lines) === JSON.stringify([[1, 0, 0, 1], [1, 192, 0, 2]]),
      JSON.stringify(map.lines),
    );
    check("event table is present", Array.isArray(map.events) && map.events.length > 0);
  }
}

// --- 2. -g must not change the SMF --------------------------------------
{
  const a = await compile(["song.mml", "a.mid"], { "song.mml": SRC });
  const b = await compile(["-g2", "song.mml", "b.mid"], { "song.mml": SRC });
  const ma = a.read("a.mid"), mb = b.read("b.mid");
  check(
    "-g2 leaves the SMF byte-identical",
    ma !== null && mb !== null && Buffer.compare(Buffer.from(ma), Buffer.from(mb)) === 0,
  );
}

// --- 3. --diag=json ------------------------------------------------------
{
  const r = await compile(["--diag=json", "bad.mml", "bad.mid"], {
    "bad.mml": "A C1 o4 l4 c\n#nosuchdirective\n",
  });
  check("a bad MML fails", r.status !== 0, `status=${r.status}`);
  const line = r.err.split("\n").find((s) => s.startsWith("{"));
  check("reported a JSON diagnostic", line !== undefined, r.err);
  if (line) {
    const d = JSON.parse(line);
    check("diagnostic is an error on line 2", d.severity === "error" && d.line === 2,
      line);
  }
}

// --- 4. #include through NODEFS -----------------------------------------
// The extension mounts the real workspace this way, so exercise it here.
{
  const mod = await createMml2mid({ noInitialRun: true, printErr: () => {} });
  const here = new URL(".", import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, "$1");
  try {
    mod.FS.mkdir("/work");
    mod.FS.mount(mod.NODEFS, { root: here }, "/work");
    check("NODEFS mounts a real directory", true);
    check(
      "can read through the mount",
      mod.FS.readFile("/work/smoke.mjs").length > 0,
    );
  } catch (e) {
    check("NODEFS mounts a real directory", false, String(e));
  }
}

process.exitCode = failures ? 1 : 0;
console.log(failures ? `\n${failures} CHECK(S) FAILED` : "\nall checks passed");
