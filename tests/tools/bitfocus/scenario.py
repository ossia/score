#!/usr/bin/env python3
# Runs hand-written scenarios (scenarios/*.json): explicit configuration and
# action options, an optional device persona in the catcher, and packets the
# device sends back. Compares the reference host and score action by action.
#
#   tests/tools/bitfocus/scenario.py tests/tools/bitfocus/scenarios/*.json \
#       --driver <build>/bitfocus_module_driver --out out
#
# A scenario's "persona" makes the catcher act as that device (see catcher.py),
# and "inject" packets are sent to the module on 127.0.0.1 after the actions.
import argparse, json, os, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import analyze as A

ap = argparse.ArgumentParser()
ap.add_argument("scenarios", nargs="+")
ap.add_argument("--driver", required=True)
ap.add_argument("--out", required=True)
ap.add_argument("--modules", default=os.path.expanduser(
    "~/Documents/ossia/score/packages/companion-modules/companion-bundled-modules"))
ap.add_argument("--node-runtime", default=os.path.expanduser(
    "~/Documents/ossia/score/packages/companion-modules/node-runtime"))
ap.add_argument("--gap", default="300")
args = ap.parse_args()


def run(cmd, log, env):
    with open(log, "w") as err:
        return subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=err, env=env).returncode


def payloads(catch, t0, t1):
    return [(e["p"], e["port"], bytes.fromhex(e["d"])) for e in catch
            if e.get("ev") == "data" and t0 <= e["t"] < t1]


failures = 0
for sc_path in args.scenarios:
    sc_path = Path(sc_path).resolve()
    sc = json.load(open(sc_path))
    d = Path(args.out).resolve() / sc_path.stem
    d.mkdir(parents=True, exist_ok=True)
    mdir = str(Path(args.modules) / sc["module"])
    env = dict(os.environ, BITFOCUS_NODE_RUNTIME=args.node_runtime, SCORE_BITFOCUS_TRACE="1",
               NETNS_LOOPBACK_DIRECT="1", CATCHER_PERSONA=sc.get("persona", ""))
    for name in ("ref", "ref2"):
        run([str(HERE / "netns-run.sh"), str(d / f"{name}.catch"), "timeout", "300", "node",
             str(HERE / "ref-host.mjs"), "--module", mdir, "--out", str(d / f"{name}.jsonl"),
             "--node-runtime", args.node_runtime, "--gap", args.gap, "--scenario", str(sc_path)],
            d / f"{name}.stderr", env)
    run([str(HERE / "netns-run.sh"), str(d / "score.catch"), "timeout", "300", args.driver,
         "--module", mdir, "--out", str(d / "score.jsonl"), "--config", str(d / "ref.jsonl.config.json"),
         "--gap", args.gap, "--scenario", str(sc_path)], d / "score.stderr", env)

    print(f"===== {sc_path.stem} ({sc['module']})")
    ref, ref2, sc_ev = A.jl(d / "ref.jsonl"), A.jl(d / "ref2.jsonl"), A.jl(d / "score.jsonl")
    cats = {n: A.jl(d / f"{n}.catch") for n in ("ref", "ref2", "score")}
    wins = {"ref": A.windows(ref), "ref2": A.windows(ref2), "score": A.windows(sc_ev)}
    _, dev_tr = A.trace(d / "score.stderr")
    sopts = [c["_payload"]["action"] for c in dev_tr if c.get("name") == "executeAction" and c["_dir"] == "->"]
    scalls = {c["callbackId"]: i for i, c in enumerate(dev_tr) if c.get("name") == "executeAction" and c["_dir"] == "->"}
    sres = {}
    for o in dev_tr:
        if o.get("direction") == "response" and o["_dir"] == "<-" and o.get("callbackId") in scalls:
            p = o["_payload"] or {}
            sres[scalls[o["callbackId"]]] = (bool(o.get("success")) and p.get("success", True), p.get("errorMessage") or p.get("message"))
    ropts = [e["options"] for e in ref if e.get("ev") == "action"]
    rres = [e for e in ref if e.get("ev") == "action-result"]
    exec_index = sorted(scalls.values())
    for i, (aid, t0, t1) in enumerate(wins["ref"]):
        r = payloads(cats["ref"], t0, t1)
        r2 = payloads(cats["ref2"], *wins["ref2"][i][1:]) if i < len(wins["ref2"]) else None
        s = payloads(cats["score"], *wins["score"][i][1:]) if i < len(wins["score"]) else None
        so = sopts[i]["options"] if i < len(sopts) else None
        ok = s == r
        noisy = r2 is not None and r2 != r
        status = "OK   " if ok else ("NOISY" if noisy else "DIFF ")
        if not ok and not noisy:
            failures += 1
        print(f"  [{status}] {aid:14s} ref={r!r}")
        if not ok:
            print(f"          {'':14s} score={s!r}")
        if json.dumps(ropts[i], sort_keys=True) != json.dumps(so, sort_keys=True):
            print(f"          options ref={ropts[i]} score={so}")
        rr = next((e for e in rres if e["id"] == aid), None)
    rsum = next((e for e in ref if e.get("ev") == "summary"), {})
    ssum = next((e for e in sc_ev if e.get("ev") == "summary"), {})
    for k, v in (rsum.get("variableValues") or {}).items():
        sv = (ssum.get("variableValues") or {}).get(k, "<none>")
        mark = "OK  " if str(sv) == str(v) else "DIFF"
        if mark == "DIFF":
            failures += 1
        print(f"  var [{mark}] {k} ref={v!r} score={sv!r}")
    tree = {c["name"]: c for c in (ssum.get("tree") or {}).get("children", [])}
    sfb = {c["name"]: c.get("value") for c in tree.get("feedback", {}).get("children", [])}
    for k, v in (rsum.get("feedbackValues") or {}).items():
        mark = "OK  " if json.dumps(sfb.get(k)) == json.dumps(v) else "DIFF"
        if mark == "DIFF":
            failures += 1
        print(f"  fb  [{mark}] {k} ref={v!r} score={sfb.get(k)!r}")
    errs_r = [e.get("message") for e in ref if e.get("ev") == "log" and e.get("phase") == "run" and e.get("level") in ("error", "warn")]
    errs_s = [((o["_payload"] or {}).get("message")) for o in dev_tr if o.get("name") == "log-message"
              and (o["_payload"] or {}).get("level") in ("error", "warn")]
    if errs_r or errs_s:
        print(f"  module errors ref={errs_r[:4]} score={errs_s[:4]}")
print(f"TOTAL non-noise differences: {failures}")
