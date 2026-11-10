#!/usr/bin/env python3
"""Every CLAP plug-in installed on the machine, through score's host.

test_clap_sweep_worker lists the plug-ins of each .clap file, then probes them
in batches: each one plays once in a reference host written from the
specification, then twice in score's host (a stop between the two, as the
transport does), has its controls set while stopped, and its state saved and
loaded back. Both hosts reach it through clap_checker.clap, which reports
what a host does that the specification forbids. Workers run in parallel; a
plug-in that crashes or hangs one is recorded as such, and the worker
restarts with the rest of its batch.

What each plug-in is decides what it is fed and what is expected of it:
  instrument      notes in, sound out
  effect          a signal in, a signal out
  generator       sound out with nothing in
  analyzer        a signal in, read-only parameters out that move
  midi-effect     notes in, notes out
  midi-generator  notes out with nothing in
Severities:
  host     score's host is the suspect: it breaks a rule of the specification
           the reference host keeps, crashes or hangs where the reference host
           did not, does worse than the reference host, or the second play
           differs from the first. These fail the ctest entry.
  bug      the plug-in misbehaves in both hosts: crash, hang, NaN
  warn     silent or unresponsive where it should not be, too loud, too slow
  info     worth knowing, often fine

Results: <out>/results.json (every record and verdict), <out>/report.md.
"""

import argparse
import json
import math
import os
import re
import subprocess
import sys
import tempfile
import time
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

SILENT = 1e-5  # -100 dBFS
LOUD = 4.0  # +12 dBFS
LEVEL_MISMATCH_DB = 12.0
LEVEL_WARN_DB = 30.0

if sys.platform == "darwin":
    DEFAULT_CLAP_PATHS = ["~/Library/Audio/Plug-Ins/CLAP", "/Library/Audio/Plug-Ins/CLAP"]
else:
    DEFAULT_CLAP_PATHS = ["~/.clap", "/usr/lib/clap", "/usr/local/lib/clap", "/usr/lib64/clap"]

# The phases of score's host, as the worker names them
SCORE_PHASES = {"load", "play", "replay", "edit", "state", "teardown"}


def clap_paths(arg):
    paths = []
    if arg:
        paths += arg.split(":")
    elif os.environ.get("CLAP_PATH"):
        paths += os.environ["CLAP_PATH"].split(":")
    paths += [os.path.expanduser(p) for p in DEFAULT_CLAP_PATHS]
    seen, res = set(), []
    for p in paths:
        p = os.path.realpath(p)
        if os.path.isdir(p) and p not in seen:
            seen.add(p)
            res.append(p)
    return res


def clap_files(paths):
    """The .clap libraries; of a macOS bundle, the binary in Contents/MacOS,
    the path score's host keeps"""
    files = []
    for root in paths:
        for dirpath, dirnames, filenames in os.walk(root):
            if sys.platform == "darwin":
                for d in dirnames:
                    if d.endswith(".clap"):
                        inner = os.path.join(dirpath, d, "Contents", "MacOS", d[: -len(".clap")])
                        if os.path.isfile(inner):
                            files.append(inner)
                dirnames[:] = [d for d in dirnames if not d.endswith(".clap")]
            for f in filenames:
                if f.endswith(".clap"):
                    files.append(os.path.join(dirpath, f))
    return sorted(set(files))


def worker_env(args, scratch, extra):
    env = dict(os.environ)
    env.update(
        {
            "SCORE_DISABLE_AUDIOPLUGINS": "1",
            "SCORE_AUDIO_BACKEND": "dummy",
            "QT_QPA_PLATFORM": "offscreen",
            "TMPDIR": scratch,
            "XDG_CONFIG_HOME": os.path.join(scratch, "config"),
            "SCORE_CLAP_SWEEP_CHECKER": args.checker,
        }
    )
    env.update(extra)
    return env


def list_file(args, path):
    """The plug-ins of one .clap, from a worker of its own: a file that
    crashes when loaded only loses itself"""
    with tempfile.TemporaryDirectory(dir=args.out) as scratch:
        out = os.path.join(scratch, "list.tsv")
        try:
            proc = subprocess.run(
                [args.worker],
                cwd=args.build,
                env=worker_env(args, scratch, {"SCORE_CLAP_SWEEP_LIST": out, "SCORE_CLAP_SWEEP_FILE": path}),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=120,
                check=False,
            )
            code = proc.returncode
        except subprocess.TimeoutExpired:
            code = "timeout"
        plugins = []
        if os.path.exists(out):
            with open(out, encoding="utf-8", errors="replace") as f:
                for line in f:
                    parts = line.rstrip("\n").split("\t")
                    if len(parts) >= 2:
                        plugins.append(
                            {
                                "uri": parts[0] + ":::" + parts[1],
                                "path": parts[0],
                                "id": parts[1],
                                "name": parts[2] if len(parts) > 2 else "",
                                "vendor": parts[3] if len(parts) > 3 else "",
                                "version": parts[4] if len(parts) > 4 else "",
                                "features": [x for x in (parts[5] if len(parts) > 5 else "").split(";") if x],
                            }
                        )
        return plugins, code


def list_plugins(args, files):
    plugins, failed = [], []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for path, (found, code) in zip(files, pool.map(lambda f: list_file(args, f), files)):
            plugins += found
            if not found:
                failed.append({"uri": path, "path": path, "status": "list-failed", "exit": str(code)})
    return plugins, failed


class Slot:
    """One worker process and the batch it is probing"""

    def __init__(self, args, index):
        self.args = args
        self.index = index
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
        base = Path(self.args.out) / "work" / f"slot{self.index}-run{self.runs}"
        base.mkdir(parents=True, exist_ok=True)
        self.scratch = str(base)
        self.jobs = base / "jobs.tsv"
        self.results = base / "results.jsonl"
        self.log = base / "worker.log"
        self.jobs.write_text("".join(f"{p['path']}\t{p['id']}\n" for p in batch), encoding="utf-8")
        self.results.write_text("")
        self.logf = open(self.log, "wb")
        self.proc = subprocess.Popen(
            [self.args.worker],
            cwd=self.args.build,
            env=worker_env(
                self.args,
                self.scratch,
                {
                    "SCORE_CLAP_SWEEP_JOBS": str(self.jobs),
                    "SCORE_CLAP_SWEEP_RESULTS": str(self.results),
                },
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

    def log_tail(self, n=15):
        try:
            lines = self.log.read_text(errors="replace").splitlines()
        except OSError:
            return ""
        # The backtrace the worker prints when it crashes, whole
        crash = []
        for i, l in enumerate(lines):
            if l.startswith("CRASH signal"):
                crash = lines[i:]
                break
        rest = [l for l in lines if not l.startswith("CRASH")][-n:]
        return "\n".join(rest + crash[:80])


def describe_exit(code):
    if code < 0:
        try:
            import signal

            return f"signal {signal.Signals(-code).name}"
        except (ValueError, ImportError):
            return f"signal {-code}"
    return f"exit code {code}"


def run_sweep(args, plugins):
    queue = list(plugins)
    slots = [Slot(args, i) for i in range(args.jobs)]
    results = {}
    total = len(plugins)
    started = time.monotonic()
    by_uri = {p["uri"]: p for p in plugins}

    def finish(rec):
        meta = by_uri.get(rec["uri"], {})
        rec = {**meta, **{k: v for k, v in rec.items() if v not in ("", None) or k not in meta}}
        results[rec["uri"]] = rec

    def next_batch():
        batch = queue[: args.batch]
        del queue[: args.batch]
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
            hung = code is None and s.current is not None and time.monotonic() - s.last_progress > args.timeout
            if code is None and not hung:
                continue
            if hung:
                s.kill()
            for rec in s.poll_records():
                finish(rec)
            s.logf.close()
            rest = s.remaining()
            if rest and s.current is None and s.phase is None:
                # Gone before it began a plug-in: no plug-in to blame
                for o in slots:
                    if o.proc and o is not s:
                        o.kill()
                raise SystemExit(f"the worker failed before probing anything ({describe_exit(code)}):\n" + s.log_tail(40))
            if rest:
                # The plug-in being probed takes the blame; the others go back
                culprit = s.current or rest[0]["uri"]
                finish(
                    {
                        "type": "result",
                        "uri": culprit,
                        "status": "hang" if hung else "crash",
                        "phase": s.phase,
                        "exit": "timeout" if hung else describe_exit(code),
                        "log": s.log_tail(),
                        "worker_log": str(s.log),
                    }
                )
                queue[:0] = [p for p in rest if p["uri"] != culprit]
            s.proc = None
            if queue:
                s.start(next_batch())
        if args.progress:
            sys.stderr.write(f"\r{len(results)}/{total} plug-ins, {time.monotonic() - started:.0f} s   ")
            sys.stderr.flush()
    if args.progress:
        sys.stderr.write("\n")
    return results


def db(a, b):
    if a <= 0 or b <= 0:
        return math.inf
    return 20 * math.log10(a / b)


def judge(r):
    """The verdicts on one record: (severity, code, message)"""
    v = []
    add = lambda sev, code, msg: v.append({"severity": sev, "code": code, "message": msg})
    status = r.get("status")
    if status == "list-failed":
        add("warn", "list-failed", f"no plug-in could be listed from it ({r.get('exit')})")
        return v
    if status in ("crash", "hang"):
        where = r.get("phase") or "?"
        what = "crashed" if status == "crash" else "stopped progressing"
        if where in SCORE_PHASES:
            # The reference host ran it first and lived
            add("host", f"{status}-in-score", f"{what} in score's host during {where} ({r.get('exit')}), not in the reference host")
        else:
            add("bug", status, f"{what} in the reference host ({r.get('exit')})")
        return v
    if status == "instantiate-failed":
        if r.get("ref_skipped"):
            add("warn", "instantiate-failed", f"score could not instantiate it; reference host: {r['ref_skipped']}")
        else:
            add("host", "instantiate-failed", "score could not instantiate it, the reference host could")
        return v

    ref_codes = {x["code"] for x in r.get("ref_violations") or []}
    for x in r.get("score_violations") or []:
        if x["code"] in ref_codes:
            add("info", "checker-both", f"{x['code']} in both hosts: {x['message']}")
        else:
            add("host", "spec:" + x["code"], f"{x['message']} ({x['count']}x)")
    for x in r.get("ref_violations") or []:
        add("info", "ref-violation", f"reference host: {x['code']}: {x['message']}")

    for key in ("play_error", "replay_error"):
        if r.get(key):
            add("host", key.replace("_", "-"), r[key])

    role = r.get("role", "other")
    ports = r.get("ports", {})
    ref_ran = "ref_rms" in r
    # What comes out after the first 0.1 s: a blip while starting up is not sound
    late = lambda k: r.get(k + "_peak_late", r.get(k + "_peak", 0)) or 0
    play_peak, replay_peak, ref_peak = late("play"), late("replay"), late("ref")
    has_audio_out = len(ports.get("audio_out", [])) > 0

    if (r.get("play_nonfinite") or 0) > 0:
        if ref_ran and (r.get("ref_nonfinite") or 0) == 0:
            add("host", "nonfinite", "NaN or infinite samples, none in the reference host")
        else:
            add("bug", "nonfinite", "NaN or infinite samples in both hosts")
    elif (r.get("replay_nonfinite") or 0) > 0:
        add("host", "nonfinite-on-replay", "NaN or infinite samples only on the second play")
    if (r.get("play_controls_nonfinite") or 0) > 0:
        add("warn", "control-nonfinite", "NaN or infinite parameter output values")

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
        elif replay_peak < SILENT:
            add("host", "silent-on-replay", f"sound on the first play (peak {play_peak:.3g}), silent on the second")
        elif ref_ran and ref_peak >= SILENT:
            diff = db(r.get("play_rms", 0), r.get("ref_rms", 0))
            if abs(diff) > LEVEL_WARN_DB:
                add("warn", "level-mismatch", f"{diff:+.1f} dB from the reference host")
            elif abs(diff) > LEVEL_MISMATCH_DB:
                add("info", "level-mismatch", f"{diff:+.1f} dB from the reference host")
        if role == "instrument" and play_peak >= SILENT and (r.get("play_peak_tail") or 0) >= 0.5 * play_peak and (r.get("play_rms_held") or 0) > 0:
            if ref_ran and (r.get("ref_peak_tail") or 0) < 0.1 * ref_peak:
                add("host", "no-release", "as loud at the end as while notes were held, unlike in the reference host: notes may hang")
            else:
                add("info", "no-release", "as loud at the end as while notes were held")

    full_peak = r.get("play_peak", 0) or 0
    if has_audio_out and full_peak > LOUD:
        add("warn", "loud", f"peak {full_peak:.3g} ({20 * math.log10(full_peak):+.1f} dBFS)")
    rms = r.get("play_rms", 0) or 0
    dc = r.get("play_dc", 0) or 0
    if has_audio_out and abs(dc) > 0.05 and abs(dc) > 0.5 * rms:
        add("warn", "dc", f"DC offset {dc:+.3f}")
    if role == "effect" and r.get("play_max_diff_input") is not None and r["play_max_diff_input"] < 1e-6:
        add("info", "passthrough", "passthrough at its default settings")

    if ports.get("param_out", 0) > 0:
        if (r.get("play_controls_changed") or 0) == 0 and ref_ran and (r.get("ref_controls_changed") or 0) > 0:
            add("host", "outputs-still-in-score", "read-only parameters do not move in score, they do in the reference host")
        elif role == "analyzer" and (r.get("play_controls_changed") or 0) == 0:
            add("warn", "analyzer-still", "fed a signal, none of its outputs moves")
    if role == "midi-effect" and (r.get("play_midi_out") or 0) == 0:
        if ref_ran and (r.get("ref_midi_out") or 0) > 0:
            add("host", "no-midi-in-score", "no notes out in score, some in the reference host")
        else:
            add("warn", "no-midi", "notes in, no notes out")
    if role == "midi-generator" and (r.get("play_midi_out") or 0) == 0:
        add("info", "no-midi", "no notes out with the transport playing")

    if r.get("edit_mismatch"):
        add("host", "edit-mismatch", "controls set in score do not reach their parameter: " + "; ".join(r["edit_mismatch"][:4]))
    sc, rsc = r.get("state_changed"), r.get("ref_state_changed")
    if sc:
        if rsc:
            add("bug", "state-roundtrip", f"{sc} parameters change through a state save and load, in both hosts")
        else:
            add("host", "state-roundtrip", f"{sc} parameters change through a preset of its state, not through a state save and load in the reference host")

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

    roles = Counter(r.get("role", r.get("status")) for r in records)
    sev_count = Counter()
    by_code = defaultdict(list)
    for r in records:
        for v in r["verdicts"]:
            sev_count[v["severity"]] += 1
            by_code[(v["severity"], v["code"])].append((r, v))
    clean = sum(1 for r in records if not any(v["severity"] != "info" for v in r["verdicts"]))

    def label(r):
        name = r.get("name") or r["uri"]
        return f"**{name}** `{os.path.basename(r.get('path', ''))}:{r.get('id', '')}`"

    lines = ["# CLAP system sweep", ""]
    lines.append(f"{len(records)} plug-ins, {clean} without a host, bug or warn verdict.")
    lines.append("")
    lines.append("| role | plug-ins |")
    lines.append("|---|---|")
    for role, n in roles.most_common():
        lines.append(f"| {role} | {n} |")
    lines.append("")
    lines.append("| severity | verdicts | plug-ins |")
    lines.append("|---|---|---|")
    for sev in SEVERITIES:
        n = sum(1 for r in records if any(v["severity"] == sev for v in r["verdicts"]))
        lines.append(f"| {sev} | {sev_count[sev]} | {n} |")
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
                lines.append(f"- {label(r)} ({r.get('role', '?')}): {v['message']}")
                if sev in ("host", "bug") and r.get("log") and r.get("status") in ("crash", "hang"):
                    lines.append("")
                    lines.append("  ```")
                    lines += ["  " + l for l in r["log"].splitlines()[-40:]]
                    lines.append("  ```")
            lines.append("")
    Path(args.out, "report.md").write_text("\n".join(lines))

    print(f"{len(records)} plug-ins; " + ", ".join(f"{s}: {sev_count[s]}" for s in SEVERITIES))
    for sev in ("host", "bug"):
        for k in sorted(k for k in by_code if k[0] == sev):
            for r, v in by_code[k]:
                print(f"  [{sev}] {r.get('name') or r['uri']}: {v['message']}")
    print(f"report: {Path(args.out, 'report.md')}")
    return sev_count["host"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--worker", required=True, help="the test_clap_sweep_worker executable")
    ap.add_argument("--checker", required=True, help="clap_checker.clap")
    ap.add_argument("--build", default=os.getcwd(), help="score's build directory, where workers run")
    ap.add_argument("--out", default="clap-sweep", help="where results go")
    ap.add_argument("--jobs", type=int, default=max(1, min(4, (os.cpu_count() or 2) // 4)))
    ap.add_argument("--batch", type=int, default=20, help="plug-ins per worker process")
    ap.add_argument("--timeout", type=float, default=120, help="seconds without progress before a hang")
    ap.add_argument("--filter", help="only the plug-ins whose path, id or name matches this regex")
    ap.add_argument("--limit", type=int, help="at most this many plug-ins")
    ap.add_argument("--clap-path", help="':'-separated directories to sweep; $CLAP_PATH and the system ones by default")
    ap.add_argument("--rejudge", action="store_true", help="judge <out>/results.json again, no sweep")
    ap.add_argument("--require-opt-in", action="store_true", help="skip (77) unless SCORE_CLAP_SWEEP=1")
    ap.add_argument("--no-progress", dest="progress", action="store_false")
    args = ap.parse_args()

    if args.require_opt_in and os.environ.get("SCORE_CLAP_SWEEP") != "1":
        print("skipped: set SCORE_CLAP_SWEEP=1 to sweep the CLAP plug-ins of this machine")
        return 77

    args.out = os.path.abspath(args.out)
    os.makedirs(args.out, exist_ok=True)

    if args.rejudge:
        records = json.loads(Path(args.out, "results.json").read_text())
        return 1 if report(args, {r["uri"]: r for r in records}) else 0

    args.worker = os.path.abspath(args.worker)
    args.checker = os.path.abspath(args.checker)
    paths = clap_paths(args.clap_path)
    files = clap_files(paths)
    rx = re.compile(args.filter, re.I) if args.filter else None
    plugins, failed = list_plugins(args, files)
    if rx:
        plugins = [p for p in plugins if rx.search(p["path"]) or rx.search(p["id"]) or rx.search(p["name"])]
        failed = [f for f in failed if rx.search(f["path"])]
    if args.limit:
        plugins = plugins[: args.limit]
    if not plugins:
        print(f"no CLAP plug-in found in {':'.join(paths)}")
        return 77
    print(f"{len(plugins)} plug-ins in {len(files)} files under {':'.join(paths)}, {args.jobs} workers")
    results = run_sweep(args, plugins)
    for f in failed:
        results[f["uri"]] = f
    return 1 if report(args, results) else 0


if __name__ == "__main__":
    sys.exit(main())
