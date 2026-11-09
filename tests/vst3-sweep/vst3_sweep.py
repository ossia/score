#!/usr/bin/env python3
"""Every VST3 plug-in installed on the machine, through score's host.

test_vst3_sweep_worker first loads each bundle to list its audio classes, then
probes the classes in batches: each one plays twice in score's host (a stop
between the two, as the transport does: the notes off, then the node torn
down) and once in a reference host written against the SDK's hosting classes.
Workers run in parallel; a bundle or a plug-in that crashes or hangs one is
recorded as such, and the worker restarts with the rest of its batch.

What each plug-in is decides what it is fed and what is expected of it:
  instrument      notes in, sound out
  effect          a signal in, a signal out
  generator       sound out with nothing in
  analyzer        a signal in, parameter values out that move
  midi-effect     notes in, events out
  midi-generator  events out with nothing in
Severities:
  host     score's host is the suspect: the reference host does better, the
           second play differs from the first, or it breaks in score's stop.
           These fail the ctest entry.
  bug      the plug-in misbehaves in both hosts: crash, hang, NaN
  warn     silent or unresponsive where it should not be, too loud, too slow
  info     worth knowing, often fine: bit-exact at defaults, slow to load

Results: <out>/results.json (every record and verdict), <out>/report.md.
"""

import argparse
import json
import math
import os
import re
import subprocess
import sys
import time
from collections import Counter, defaultdict
from pathlib import Path

SILENT = 1e-5  # -100 dBFS
LOUD = 4.0  # +12 dBFS
LEVEL_MISMATCH_DB = 12.0
DIFFERS_DB = 3.0
UNSTABLE_DB = 40.0

DEFAULT_VST3_PATHS = [
    "~/.vst3",
    "/usr/lib/vst3",
    "/usr/local/lib/vst3",
    "/usr/lib64/vst3",
    "/usr/local/lib64/vst3",
]


def vst3_paths(arg):
    if arg:
        return arg.split(":")
    if os.environ.get("VST3_PATH"):
        return os.environ["VST3_PATH"].split(":")
    paths = [os.path.expanduser(p) for p in DEFAULT_VST3_PATHS]
    return [p for p in paths if os.path.isdir(p)]


def find_bundles(paths):
    bundles = []
    # /usr/lib64 is often /usr/lib
    seen = set()
    for root in paths:
        if os.path.realpath(root) in seen:
            continue
        seen.add(os.path.realpath(root))
        for dirpath, dirnames, _ in os.walk(root, followlinks=True):
            for d in sorted(dirnames):
                if d.endswith(".vst3"):
                    bundles.append(os.path.join(dirpath, d))
            # A bundle's own contents are not more bundles
            dirnames[:] = [d for d in dirnames if not d.endswith(".vst3")]
    return sorted(bundles)


def worker_env(args, scratch, extra):
    env = dict(os.environ)
    env.update(
        {
            "SCORE_DISABLE_AUDIOPLUGINS": "1",
            "SCORE_AUDIO_BACKEND": "dummy",
            "QT_QPA_PLATFORM": "offscreen",
            "TMPDIR": scratch,
            "XDG_CONFIG_HOME": os.path.join(scratch, "config"),
        }
    )
    env.update(extra)
    return env


class Slot:
    """One worker process and the batch it is working through"""

    def __init__(self, args, index, stage, env_var, line):
        self.args = args
        self.index = index
        self.stage = stage
        self.env_var = env_var
        self.line = line
        self.proc = None
        self.batch = []
        self.done = set()
        self.current = None
        self.phase = None
        self.last_progress = 0.0
        self.offset = 0
        self.runs = 0

    def start(self, batch):
        self.runs += 1
        self.batch = batch
        self.done = set()
        self.current = None
        self.phase = None
        self.offset = 0
        base = Path(self.args.out) / "work" / f"{self.stage}-slot{self.index}-run{self.runs}"
        base.mkdir(parents=True, exist_ok=True)
        self.scratch = str(base)
        self.jobs = base / "jobs.tsv"
        self.results = base / "results.jsonl"
        self.log = base / "worker.log"
        self.jobs.write_text("".join(self.line(p) + "\n" for p in batch), encoding="utf-8")
        self.results.write_text("")
        self.logf = open(self.log, "wb")
        self.proc = subprocess.Popen(
            [self.args.worker],
            cwd=self.args.build,
            env=worker_env(
                self.args,
                self.scratch,
                {self.env_var: str(self.jobs), "SCORE_VST3_SWEEP_RESULTS": str(self.results)},
            ),
            stdout=self.logf,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
        self.last_progress = time.monotonic()

    def poll_records(self):
        with open(self.results, encoding="utf-8", errors="replace") as f:
            f.seek(self.offset)
            data = f.read()
        # Only whole lines: the worker may be writing the next one
        end = data.rfind("\n")
        if end < 0:
            return []
        self.offset += len(data[: end + 1].encode("utf-8"))
        out = []
        for line in data[: end + 1].splitlines():
            try:
                rec = json.loads(line)
            except json.JSONDecodeError:
                continue
            self.last_progress = time.monotonic()
            if rec.get("type") == "phase":
                self.current = rec["uri"]
                self.phase = rec["phase"]
            elif rec.get("type") == "result":
                self.done.add(rec["uri"])
                self.current = None
                out.append(rec)
        return out

    def remaining(self):
        return [p for p in self.batch if p["uri"] not in self.done]

    def kill(self):
        try:
            os.killpg(self.proc.pid, 9)
        except ProcessLookupError:
            pass
        self.proc.wait()

    def crashed_in_earlier(self, current_bundle):
        """The plug-in of this batch, already probed, in whose bundle the
        backtrace of the crash starts, if not the one being probed"""
        try:
            lines = self.log.read_text(errors="replace").splitlines()
        except OSError:
            return None
        for l in lines:
            m = re.search(r"^\s*#\d+ .*?(/\S+?\.vst3)/Contents/", l)
            if not m:
                continue
            bundle = m.group(1)
            if bundle == current_bundle:
                return None
            done = [p["uri"] for p in self.batch if p.get("bundle") == bundle and p["uri"] in self.done]
            return done[-1] if done else None
        return None

    def log_tail(self, n=15):
        try:
            lines = self.log.read_text(errors="replace").splitlines()
        except OSError:
            return ""
        return "\n".join(lines[-n:])


def describe_exit(code):
    if code < 0:
        try:
            import signal

            return f"signal {signal.Signals(-code).name}"
        except (ValueError, ImportError):
            return f"signal {-code}"
    return f"exit code {code}"


def run_stage(args, items, stage, env_var, line, batch_size):
    """Runs the worker over items (dicts with a "uri") in batches, in parallel"""
    queue = list(items)
    slots = [Slot(args, i, stage, env_var, line) for i in range(args.jobs)]
    results = {}
    total = len(items)
    started = time.monotonic()
    by_uri = {p["uri"]: p for p in items}

    def finish(rec):
        meta = by_uri.get(rec["uri"], {})
        results[rec["uri"]] = {**meta, **rec}

    def next_batch():
        batch = queue[:batch_size]
        del queue[:batch_size]
        return batch

    for s in slots:
        if queue:
            s.start(next_batch())

    while any(s.proc for s in slots):
        time.sleep(0.2)
        for s in slots:
            if not s.proc:
                continue
            for rec in s.poll_records():
                finish(rec)
            code = s.proc.poll()
            hung = (
                code is None
                and s.current is not None
                and time.monotonic() - s.last_progress > args.timeout
            )
            if code is None and not hung:
                continue
            if hung:
                s.kill()
            for rec in s.poll_records():
                finish(rec)
            s.logf.close()
            rest = s.remaining()
            if rest and s.current is None and s.phase is None:
                # Gone before it began anything: nothing to blame
                for o in slots:
                    if o.proc and o is not s:
                        o.kill()
                raise SystemExit(
                    f"the worker failed before starting ({describe_exit(code)}):\n"
                    + s.log_tail(40)
                )
            if rest:
                # The item being worked on takes the blame, unless the crash
                # is in another plug-in's code: one probed before, whose
                # timer or thread outlived it. The others go back.
                culprit = s.current or rest[0]["uri"]
                phase = s.phase
                if not hung:
                    earlier = s.crashed_in_earlier(by_uri.get(culprit, {}).get("bundle"))
                    if earlier:
                        queue[:0] = [by_uri[culprit]] if culprit in by_uri else []
                        culprit, phase = earlier, "after-teardown"
                finish(
                    {
                        "type": "result",
                        "uri": culprit,
                        "status": "hang" if hung else "crash",
                        "phase": phase,
                        "exit": "timeout" if hung else describe_exit(code),
                        "log": s.log_tail(),
                        "workdir": s.scratch,
                    }
                )
                queue[:0] = [p for p in rest if p["uri"] != culprit and p not in queue]
            s.proc = None
            if queue:
                s.start(next_batch())
        if args.progress:
            sys.stderr.write(
                f"\r{stage}: {len(results)}/{total}, {time.monotonic() - started:.0f} s   "
            )
            sys.stderr.flush()
    if args.progress:
        sys.stderr.write("\n")
    return results


def scan(args, bundles):
    """The audio classes of every bundle, and the bundles that failed to load"""
    items = [{"uri": b} for b in bundles]
    res = run_stage(args, items, "scan", "SCORE_VST3_SWEEP_SCAN", lambda p: p["uri"], 20)
    plugins, failed = [], []
    for b in bundles:
        r = res.get(b, {"uri": b, "status": "crash", "phase": "scan"})
        if r.get("status") != "scanned":
            failed.append(r)
            continue
        for c in r.get("classes", []):
            plugins.append(
                {
                    "uri": f"{b}::{c['uid']}",
                    "bundle": b,
                    "uid": c["uid"],
                    "name": c.get("name", ""),
                    "vendor": c.get("vendor", ""),
                    "subcategories": c.get("subcategories", ""),
                }
            )
    return plugins, failed


def db(a, b):
    if a <= 0 or b <= 0:
        return math.inf
    return 20 * math.log10(a / b)


def judge(r):
    """The verdicts on one record: (severity, code, message)"""
    v = []
    add = lambda sev, code, msg: v.append({"severity": sev, "code": code, "message": msg})
    status = r.get("status")
    if r.get("stage") == "scan":
        if status in ("crash", "hang"):
            add("bug", f"scan-{status}", f"loading the bundle: {r.get('exit')} during {r.get('phase')}")
        else:
            add("warn", "load-failed", f"the bundle does not load: {r.get('error', '?')}")
        return v
    if status in ("crash", "hang"):
        where = r.get("phase") or "?"
        what = f"{r.get('exit')} during {where}"
        if where in ("replay", "stop", "restop"):
            # The first play went through: what breaks is score's stop (notes
            # off, then the node torn down) or the node made again after it
            add("host", f"{status}-on-{where}", f"{status}: {what}, the first play went through")
        elif where == "reference":
            add("info", f"{status}-in-reference", what)
        else:
            add("bug", status, what)
        return v
    if status == "not-found":
        add("warn", "not-found", "score's host did not find the class in its bundle")
        return v
    if status == "instantiate-failed":
        add("warn", "instantiate-failed", "score's host could not instantiate it")
        return v

    role = r.get("role", "other")
    ports = r.get("ports", {})
    ref_ran = "ref_rms" in r
    play_peak, replay_peak = r.get("play_peak", 0) or 0, r.get("replay_peak", 0) or 0
    ref_peak = r.get("ref_peak", 0) or 0
    has_audio_out = bool(ports.get("audio_out")) and ports["audio_out"][0] > 0

    play_nf = (r.get("play_nonfinite") or 0) > 0
    replay_nf = (r.get("replay_nonfinite") or 0) > 0
    if play_nf:
        if ref_ran and (r.get("ref_nonfinite") or 0) == 0:
            add("host", "nonfinite", "NaN or infinite samples, none in the reference host")
        else:
            add("bug", "nonfinite", "NaN or infinite samples in both hosts")
    elif replay_nf:
        add("host", "nonfinite-after-stop", "NaN or infinite samples only on the second play")
    if (r.get("play_params_nonfinite") or 0) > 0:
        add("warn", "param-nonfinite", "NaN or infinite parameter values reported")
    if (r.get("play_params_out_of_range") or 0) > 0:
        add("warn", "param-out-of-range", "reports normalized parameter values outside [0, 1]")

    if has_audio_out and role in ("instrument", "effect", "generator"):
        if play_peak < SILENT:
            if ref_ran and ref_peak >= SILENT:
                add("host", "silent-in-score", f"silent in score, peak {ref_peak:.3g} in the reference host")
            else:
                why = {
                    "instrument": "silent while notes are held",
                    "effect": "silent while fed a signal",
                    "generator": "silent with nothing to drive it",
                }[role]
                if r.get("ref_skipped"):
                    why += f"; reference host: {r['ref_skipped']}"
                add("warn", "silent", why)
        elif replay_peak < SILENT and max(r.get("play_rms_held") or 0, r.get("play_peak_tail") or 0) >= SILENT:
            # Sound all along the first play, not only at its start: a plug-in
            # fading out on its own is as silent the second time
            add("host", "silent-on-replay", f"sound on the first play (peak {play_peak:.3g}), silent on the second")
        elif ref_ran and ref_peak >= SILENT:
            diff = db(r.get("play_rms", 0), r.get("ref_rms", 0))
            if abs(diff) > UNSTABLE_DB and max(play_peak, ref_peak) > LOUD:
                add("warn", "unstable", f"peak {play_peak:.3g} in score, {ref_peak:.3g} in the reference host: uninitialised state?")
            elif abs(diff) > LEVEL_MISMATCH_DB:
                add("info", "level-mismatch", f"{diff:+.1f} dB from the reference host")
            elif abs(diff) > DIFFERS_DB:
                add("warn", "differs", f"{diff:+.1f} dB from the reference host")
        if role == "instrument" and play_peak >= SILENT and (r.get("play_peak_tail") or 0) >= 0.5 * play_peak and (r.get("play_rms_held") or 0) > 0:
            add("info", "no-release", "as loud at the end as while notes were held: notes may hang")

    if has_audio_out and play_peak > LOUD:
        add("warn", "loud", f"peak {play_peak:.3g} ({20 * math.log10(play_peak):+.1f} dBFS)")
    rms = r.get("play_rms", 0) or 0
    dc = r.get("play_dc", 0) or 0
    if has_audio_out and abs(dc) > 0.05 and abs(dc) > 0.5 * rms:
        add("warn", "dc", f"DC offset {dc:+.3f}")
    if role == "effect" and r.get("play_max_diff_input") is not None and r["play_max_diff_input"] < 1e-7:
        add("info", "passthrough", "bit-exact passthrough at its default settings")

    if role == "analyzer":
        if (r.get("play_params_changed") or 0) == 0 and (r.get("play_events_out") or 0) == 0:
            if ref_ran and (r.get("ref_params_changed") or 0) > 0:
                add("host", "analyzer-still-in-score", "reported values do not move in score, they do in the reference host")
            else:
                add("warn", "analyzer-still", "fed a signal, reports nothing that moves")
    elif ref_ran and (r.get("ref_params_changed") or 0) > 0 and (r.get("play_params_changed") or 0) == 0:
        add("host", "params-still-in-score", f"{r['ref_params_changed']} reported parameters move in the reference host, none in score")
    if role in ("midi-effect", "midi-generator") and (r.get("play_events_out") or 0) == 0:
        if ref_ran and (r.get("ref_events_out") or 0) > 0:
            add("host", "no-events-in-score", "no events out in score, some in the reference host")
        else:
            add("warn" if role == "midi-effect" else "info", "no-events", "no events out")
    elif ports.get("event_out") and (r.get("play_events_out") or 0) == 0 and (r.get("ref_events_out") or 0) > 0:
        add("host", "no-events-in-score", "no events out in score, some in the reference host")

    rt = r.get("play_rt_factor") or 0
    if rt > 1.0:
        add("warn", "slow", f"{rt:.2f}x real time in score's host")
    if (r.get("instantiate_ms") or 0) > 5000:
        add("info", "slow-load", f"{r['instantiate_ms'] / 1000:.1f} s to instantiate")
    if not ref_ran and r.get("ref_skipped"):
        add("info", "ref-skipped", f"reference host: {r['ref_skipped']}")
    return v


SEVERITIES = ["host", "bug", "warn", "info"]


def report(args, results):
    records = sorted(results.values(), key=lambda r: ((r.get("name") or r["uri"]).lower(), r["uri"]))
    for r in records:
        r["verdicts"] = judge(r)
    Path(args.out, "results.json").write_text(json.dumps(records, indent=1))

    roles = Counter(r.get("role", r.get("stage") == "scan" and "bundle" or r.get("status")) for r in records)
    sev_count = Counter()
    by_code = defaultdict(list)
    role_sev = defaultdict(Counter)
    for r in records:
        for v in r["verdicts"]:
            sev_count[v["severity"]] += 1
            by_code[(v["severity"], v["code"])].append((r, v))
        sevs = {v["severity"] for v in r["verdicts"]}
        worst = next((s for s in SEVERITIES if s in sevs), "clean")
        role_sev[r.get("role", r.get("status", "?"))][worst] += 1
    clean = sum(1 for r in records if not any(v["severity"] != "info" for v in r["verdicts"]))

    lines = ["# VST3 system sweep", ""]
    lines.append(f"{len(records)} records, {clean} without a host, bug or warn verdict.")
    lines.append("")
    lines.append("Worst verdict of each plug-in, by role:")
    lines.append("")
    lines.append("| role | plug-ins | host | bug | warn | info | clean |")
    lines.append("|---|---|---|---|---|---|---|")
    for role, n in roles.most_common():
        c = role_sev[role]
        lines.append(f"| {role} | {n} | " + " | ".join(str(c[s]) for s in SEVERITIES + ["clean"]) + " |")
    lines.append("")
    for sev in SEVERITIES:
        codes = sorted((k for k in by_code if k[0] == sev), key=lambda k: -len(by_code[k]))
        if not codes:
            continue
        lines.append(f"## {sev} ({sev_count[sev]})")
        lines.append("")
        for k in codes:
            items = by_code[k]
            lines.append(f"### {k[1]} ({len(items)})")
            lines.append("")
            for r, v in items:
                name = r.get("name") or r["uri"]
                lines.append(f"- **{name}** `{r['uri']}` ({r.get('role', '?')}): {v['message']}")
            lines.append("")
    Path(args.out, "report.md").write_text("\n".join(lines))

    print(f"{len(records)} records; " + ", ".join(f"{s}: {sev_count[s]}" for s in SEVERITIES))
    for sev in ("host", "bug"):
        for k in sorted(k for k in by_code if k[0] == sev):
            for r, v in by_code[k]:
                print(f"  [{sev}] {r.get('name') or r['uri']}: {v['message']}")
    print(f"report: {Path(args.out, 'report.md')}")
    return sev_count["host"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--worker", required=True, help="the test_vst3_sweep_worker executable")
    ap.add_argument("--build", default=os.getcwd(), help="score's build directory, where workers run")
    ap.add_argument("--out", default="vst3-sweep", help="where results go")
    ap.add_argument("--jobs", type=int, default=max(1, min(4, (os.cpu_count() or 2) // 2)))
    ap.add_argument("--batch", type=int, default=20, help="plug-ins per worker process")
    ap.add_argument("--timeout", type=float, default=120, help="seconds without progress before a hang")
    ap.add_argument("--filter", help="only the plug-ins whose bundle, UID or name matches this regex")
    ap.add_argument("--limit", type=int, help="at most this many plug-ins")
    ap.add_argument("--vst3-path", help="':'-separated directories to sweep; the system ones by default")
    ap.add_argument("--rejudge", action="store_true", help="judge <out>/results.json again, no sweep")
    ap.add_argument("--require-opt-in", action="store_true", help="skip (77) unless SCORE_VST3_SWEEP=1")
    ap.add_argument("--no-progress", dest="progress", action="store_false")
    args = ap.parse_args()

    if args.require_opt_in and os.environ.get("SCORE_VST3_SWEEP") != "1":
        print("skipped: set SCORE_VST3_SWEEP=1 to sweep the VST3 plug-ins of this machine")
        return 77

    args.out = os.path.abspath(args.out)
    os.makedirs(args.out, exist_ok=True)

    if args.rejudge:
        records = json.loads(Path(args.out, "results.json").read_text())
        return 1 if report(args, {r["uri"]: r for r in records}) else 0

    args.worker = os.path.abspath(args.worker)
    paths = vst3_paths(args.vst3_path)
    bundles = find_bundles(paths)
    if args.filter:
        rx = re.compile(args.filter, re.I)
        # A filter naming no bundle may still name a class in one of them
        bundles_kept = [b for b in bundles if rx.search(b)] or bundles
    else:
        bundles_kept = bundles
    if not bundles_kept:
        print(f"no VST3 bundle found in {':'.join(paths)}")
        return 77
    print(f"{len(bundles_kept)} bundles in {':'.join(paths)}, {args.jobs} workers")
    plugins, failed = scan(args, bundles_kept)
    if args.filter:
        plugins = [
            p for p in plugins
            if rx.search(p["bundle"]) or rx.search(p["uid"]) or rx.search(p["name"])
        ]
        failed = [f for f in failed if rx.search(f["uri"])]
    if args.limit:
        plugins = plugins[: args.limit]
    print(f"{len(plugins)} plug-ins, {len(failed)} bundles that did not scan")
    results = run_stage(
        args, plugins, "probe", "SCORE_VST3_SWEEP_JOBS",
        lambda p: f"{p['bundle']}\t{p['uid']}", args.batch,
    )
    for f in failed:
        results[f["uri"]] = {**f, "stage": "scan"}
    return 1 if report(args, results) else 0


if __name__ == "__main__":
    sys.exit(main())
