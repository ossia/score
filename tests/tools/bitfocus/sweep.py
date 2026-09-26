#!/usr/bin/env python3
# Runs every companion module through the reference host and through score's
# driver, each inside its own network namespace with a catcher as mock device.
#
#   npm install --prefix tests/tools/bitfocus
#   tests/tools/bitfocus/sweep.py --driver score=<build>/bitfocus_module_driver --out out --ref2
#   tests/tools/bitfocus/analyze.py out score-score
#   python3 tests/tools/bitfocus/summarize.py out score-score
#
# ref-host.mjs reproduces companion's host for 1.x modules (ChildHandlerLegacy)
# and is the oracle. --ref2 runs it twice, so that output which differs between
# two companion runs is not counted against score. Needs unshare, nft and node.
import argparse, concurrent.futures as cf, os, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ap = argparse.ArgumentParser()
ap.add_argument("--modules", default=os.path.expanduser(
    "~/Documents/ossia/score/packages/companion-modules/companion-bundled-modules"))
ap.add_argument("--node-runtime", default=os.path.expanduser(
    "~/Documents/ossia/score/packages/companion-modules/node-runtime"))
ap.add_argument("--driver", action="append", required=True, help="name=path")
ap.add_argument("--out", required=True)
ap.add_argument("--jobs", type=int, default=12)
ap.add_argument("--only", nargs="*")
ap.add_argument("--skip-ref", action="store_true")
ap.add_argument("--gap", default="80")
ap.add_argument("--ref2", action="store_true", help="run the reference a second time, to measure noise")
ap.add_argument("--seed-from", help="reuse ref files from this earlier sweep")
args = ap.parse_args()

out = Path(args.out).resolve()
out.mkdir(parents=True, exist_ok=True)
mods = sorted(p.name for p in Path(args.modules).iterdir() if (p / "companion/manifest.json").exists())
if args.only:
    mods = [m for m in mods if m in args.only]


def run(mod):
    d = out / mod
    d.mkdir(exist_ok=True)
    mdir = str(Path(args.modules) / mod)
    t0 = time.time()
    if args.seed_from and not (d / "ref.jsonl.config.json").exists():
        import shutil
        for f in Path(args.seed_from, mod).glob("ref*"):
            shutil.copy(f, d / f.name)
    if not args.skip_ref or not (d / "ref.jsonl.config.json").exists():
        with open(d / "ref.stderr", "w") as err:
            subprocess.run([str(HERE / "netns-run.sh"), str(d / "ref.catch"), "timeout", "300", "node",
                            str(HERE / "ref-host.mjs"), "--module", mdir, "--out", str(d / "ref.jsonl"),
                            "--node-runtime", args.node_runtime, "--gap", args.gap],
                           stdout=subprocess.DEVNULL, stderr=err)
    if args.ref2 and not (d / "ref2.jsonl").exists():
        with open(d / "ref2.stderr", "w") as err:
            subprocess.run([str(HERE / "netns-run.sh"), str(d / "ref2.catch"), "timeout", "300", "node",
                            str(HERE / "ref-host.mjs"), "--module", mdir, "--out", str(d / "ref2.jsonl"),
                            "--node-runtime", args.node_runtime, "--gap", args.gap],
                           stdout=subprocess.DEVNULL, stderr=err)
    env = dict(os.environ, BITFOCUS_NODE_RUNTIME=args.node_runtime, SCORE_BITFOCUS_TRACE="1")
    for drv in args.driver:
        name, path = drv.split("=", 1)
        if (d / f"score-{name}.rc").exists() and (d / f"score-{name}.jsonl").exists():
            continue
        with open(d / f"score-{name}.stderr", "w") as err:
            r = subprocess.run([str(HERE / "netns-run.sh"), str(d / f"score-{name}.catch"), "timeout", "300", path,
                                "--module", mdir, "--out", str(d / f"score-{name}.jsonl"),
                                "--config", str(d / "ref.jsonl.config.json"), "--gap", args.gap],
                               stdout=subprocess.DEVNULL, stderr=err, env=env)
        (d / f"score-{name}.rc").write_text(str(r.returncode))
    return mod, time.time() - t0


with cf.ThreadPoolExecutor(args.jobs) as ex:
    for i, (mod, dt) in enumerate(ex.map(run, mods)):
        print(f"[{i + 1}/{len(mods)}] {mod} {dt:.1f}s", flush=True)
