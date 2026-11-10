#!/usr/bin/env python3
"""Every LV2 plug-in installed on the machine, through score's host.

test_lv2_sweep_worker lists the plug-ins, then probes them in batches: each one
plays twice in score's host (a stop between the two, as the transport does) and
once in a reference host written from the specification. Workers run in
parallel; a plug-in that crashes or hangs one is recorded as such, and the
worker restarts with the rest of its batch.

What each plug-in is decides what it is fed and what is expected of it:
  instrument      notes in, sound out
  effect          a signal in, a signal out
  generator       sound out with nothing in
  cv-generator    sound out with only CV in: CV is held at 0, silence is fine
  analyzer        a signal in, control or atom values out that move
  midi-effect     notes in, MIDI out
  midi-generator  MIDI out with nothing in
Severities:
  host     score's host is the suspect: the reference host does better, or the
           second play differs from the first. These fail the ctest entry.
  bug      the plug-in misbehaves in both hosts: crash, hang, NaN
  warn     silent or unresponsive where it should not be, too loud, too slow
  info     worth knowing, often fine: needs CV, bit-exact at defaults

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
from pathlib import Path

SILENT = 1e-5  # -100 dBFS
LOUD = 4.0  # +12 dBFS
LEVEL_MISMATCH_DB = 12.0
UNSTABLE_DB = 40.0

if sys.platform == "darwin":
    DEFAULT_LV2_PATHS = ["~/Library/Audio/Plug-Ins/LV2", "/Library/Audio/Plug-Ins/LV2"]
else:
    DEFAULT_LV2_PATHS = [
        "~/.lv2",
        "/usr/lib/lv2",
        "/usr/local/lib/lv2",
        "/usr/lib64/lv2",
        "/usr/local/lib64/lv2",
    ]


def lv2_path(arg):
    if arg:
        return arg
    if os.environ.get("LV2_PATH"):
        return os.environ["LV2_PATH"]
    paths = [os.path.expanduser(p) for p in DEFAULT_LV2_PATHS]
    return ":".join(p for p in paths if os.path.isdir(p))


def worker_env(args, scratch, extra):
    env = dict(os.environ)
    env.update(
        {
            "LV2_PATH": args.lv2_path,
            "SCORE_DISABLE_AUDIOPLUGINS": "1",
            "SCORE_AUDIO_BACKEND": "dummy",
            "QT_QPA_PLATFORM": "offscreen",
            "TMPDIR": scratch,
            "XDG_CONFIG_HOME": os.path.join(scratch, "config"),
        }
    )
    env.update(extra)
    return env


def list_plugins(args):
    with tempfile.TemporaryDirectory(dir=args.out) as scratch:
        out = os.path.join(scratch, "list.tsv")
        subprocess.run(
            [args.worker],
            cwd=args.build,
            env=worker_env(args, scratch, {"SCORE_LV2_SWEEP_LIST": out}),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=600,
            check=False,
        )
        plugins = []
        with open(out, encoding="utf-8", errors="replace") as f:
            for line in f:
                parts = line.rstrip("\n").split("\t")
                if len(parts) >= 2:
                    plugins.append(
                        {
                            "uri": parts[0],
                            "bundle": parts[1],
                            "name": parts[2] if len(parts) > 2 else "",
                            "class": parts[3] if len(parts) > 3 else "",
                        }
                    )
        return plugins


class Slot:
    """One worker process and the batch it is probing"""

    def __init__(self, args, index):
        self.args = args
        self.index = index
        self.proc = None
        self.batch = []
        self.done = set()
        self.records = {}
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
        self.jobs.write_text(
            "".join(f"{p['uri']}\t{p['bundle']}\n" for p in batch), encoding="utf-8"
        )
        self.results.write_text("")
        self.logf = open(self.log, "wb")
        self.proc = subprocess.Popen(
            [self.args.worker],
            cwd=self.args.build,
            env=worker_env(
                self.args,
                self.scratch,
                {
                    "SCORE_LV2_SWEEP_JOBS": str(self.jobs),
                    "SCORE_LV2_SWEEP_RESULTS": str(self.results),
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
        keep = [l for l in lines if not l.startswith("pique ")]
        return "\n".join(keep[-n:])


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

    def finish(rec, meta):
        rec = {**meta, **rec}
        results[rec["uri"]] = rec

    def next_batch():
        batch = queue[: args.batch]
        del queue[: args.batch]
        return batch

    by_uri = {p["uri"]: p for p in plugins}
    for s in slots:
        if queue:
            s.start(next_batch())

    while any(s.proc for s in slots):
        time.sleep(0.2)
        for s in slots:
            if not s.proc:
                continue
            for rec in s.poll_records():
                finish(rec, by_uri.get(rec["uri"], {}))
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
                finish(rec, by_uri.get(rec["uri"], {}))
            s.logf.close()
            rest = s.remaining()
            if rest and s.current is None and s.phase is None:
                # Gone before it began a plug-in: no plug-in to blame
                for o in slots:
                    if o.proc and o is not s:
                        o.kill()
                raise SystemExit(
                    f"the worker failed before probing anything ({describe_exit(code)}):\n"
                    + s.log_tail(40)
                )
            if rest:
                # The plug-in being probed takes the blame; the others go back
                culprit = s.current or rest[0]["uri"]
                status = "hang" if hung else "crash"
                finish(
                    {
                        "type": "result",
                        "uri": culprit,
                        "status": status,
                        "phase": s.phase,
                        "exit": "timeout" if hung else describe_exit(code),
                        "log": s.log_tail(),
                    },
                    by_uri.get(culprit, {}),
                )
                queue[:0] = [p for p in rest if p["uri"] != culprit]
            s.proc = None
            if queue:
                s.start(next_batch())
        if args.progress:
            done = len(results)
            sys.stderr.write(
                f"\r{done}/{total} plug-ins, {time.monotonic() - started:.0f} s   "
            )
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
    if status == "crash":
        where = r.get("phase") or "?"
        if where == "replay":
            # The first play went through: what differs is the node made
            # again on the same instance, after the one-frame stop run
            add("host", "crash-on-replay", f"crashed on the second play only ({r.get('exit')})")
        else:
            sev = "info" if where == "reference" else "bug"
            add(sev, "crash", f"crashed during {where} ({r.get('exit')})")
        return v
    if status == "hang":
        where = r.get("phase") or "?"
        sev = "info" if where == "reference" else "bug"
        add(sev, "hang", f"no progress for the timeout during {where}")
        return v
    if status == "not-found":
        add("warn", "not-found", "score's host did not find it in its bundle")
        return v
    if status == "instantiate-failed":
        missing = r.get("missing_features") or []
        if missing:
            add("host", "missing-feature", "needs features score does not give: " + ", ".join(missing))
        else:
            add("warn", "instantiate-failed", "instantiation failed with every required feature given")
        return v

    role = r.get("role", "other")
    ports = r.get("ports", {})
    ref_ran = "ref_rms" in r
    play_peak, replay_peak = r.get("play_peak", 0) or 0, r.get("replay_peak", 0) or 0
    ref_peak = r.get("ref_peak", 0) or 0
    has_audio_out = ports.get("audio_out", 0) > 0

    play_nf = (r.get("play_nonfinite") or 0) > 0
    replay_nf = (r.get("replay_nonfinite") or 0) > 0
    if play_nf:
        if ref_ran and (r.get("ref_nonfinite") or 0) == 0:
            add("host", "nonfinite", "NaN or infinite samples, none in the reference host")
        else:
            add("bug", "nonfinite", "NaN or infinite samples in both hosts")
    elif replay_nf:
        # Between the two plays score runs the plug-in for one frame to tell it
        # the transport stopped: a plug-in dividing by (sample_count - 1) breaks
        add("host", "nonfinite-after-stop", "NaN or infinite samples only on the second play: the one-frame run at stop is the likely trigger")
    if (r.get("play_controls_nonfinite") or 0) > 0:
        add("warn", "control-nonfinite", "NaN or infinite control output values")

    if has_audio_out and role in ("instrument", "effect", "generator", "cv-generator"):
        if play_peak < SILENT:
            if ref_ran and ref_peak >= SILENT:
                add("host", "silent-in-score", f"silent in score, peak {ref_peak:.3g} in the reference host")
            elif role == "cv-generator":
                add("info", "needs-cv", "silent with its CV inputs at 0")
            else:
                why = {
                    "instrument": "silent while notes are held",
                    "effect": "silent while fed a signal",
                    "generator": "silent with nothing to drive it",
                }[role]
                if r.get("worker"):
                    why += " (loads through a worker: may need a file or a state)"
                if r.get("ref_skipped"):
                    why += f"; reference host: {r['ref_skipped']}"
                add("warn", "silent", why)
        elif replay_peak < SILENT:
            add("host", "silent-on-replay", f"sound on the first play (peak {play_peak:.3g}), silent on the second")
        elif ref_ran and ref_peak >= SILENT:
            diff = db(r.get("play_rms", 0), r.get("ref_rms", 0))
            if abs(diff) > UNSTABLE_DB and max(play_peak, ref_peak) > LOUD:
                # Seen to change from one run to the next: state the plug-in
                # reads before it writes it
                add("warn", "unstable", f"peak {play_peak:.3g} in score, {ref_peak:.3g} in the reference host: uninitialised state?")
            elif abs(diff) > LEVEL_MISMATCH_DB:
                add("info", "level-mismatch", f"{diff:+.1f} dB from the reference host")
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
        moved = (r.get("play_controls_changed") or 0) + (r.get("play_atom_out") or 0)
        if moved == 0 and (ports.get("control_out", 0) + ports.get("atom_out", 0)) > 0:
            if ref_ran and (r.get("ref_controls_changed") or 0) > 0:
                add("host", "analyzer-still-in-score", "outputs do not move in score, they do in the reference host")
            else:
                add("warn", "analyzer-still", "fed a signal, none of its outputs moves")
    if role == "midi-effect" and (r.get("play_midi_out") or 0) == 0:
        if ref_ran and (r.get("ref_midi_out") or 0) > 0:
            add("host", "no-midi-in-score", "no MIDI out in score, some in the reference host")
        else:
            add("warn", "no-midi", "notes in, no MIDI out")
    if role == "midi-generator" and (r.get("play_midi_out") or 0) == 0:
        add("info", "no-midi", "no MIDI out with the transport playing")

    rt = r.get("play_rt_factor") or 0
    if rt > 1.0:
        add("warn", "slow", f"{rt:.2f}x real time in score's host")
    if (r.get("instantiate_ms") or 0) > 5000:
        add("info", "slow-load", f"{r['instantiate_ms'] / 1000:.1f} s to instantiate")
    if ref_ran is False and r.get("ref_skipped"):
        add("info", "ref-skipped", f"reference host: {r['ref_skipped']}")
    return v


SEVERITIES = ["host", "bug", "warn", "info"]


def report(args, results):
    records = sorted(results.values(), key=lambda r: (r.get("name") or r["uri"]).lower())
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

    lines = ["# LV2 system sweep", ""]
    lines.append(f"{len(records)} plug-ins, {clean} without a host, bug or warn verdict.")
    lines.append("")
    lines.append("| role | plug-ins |")
    lines.append("|---|---|")
    for role, n in roles.most_common():
        lines.append(f"| {role} | {n} |")
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

    print(f"{len(records)} plug-ins; " + ", ".join(f"{s}: {sev_count[s]}" for s in SEVERITIES))
    for sev in ("host", "bug"):
        for k in sorted(k for k in by_code if k[0] == sev):
            for r, v in by_code[k]:
                print(f"  [{sev}] {r.get('name') or r['uri']}: {v['message']}")
    print(f"report: {Path(args.out, 'report.md')}")
    return sev_count["host"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--worker", required=True, help="the test_lv2_sweep_worker executable")
    ap.add_argument("--build", default=os.getcwd(), help="score's build directory, where workers run")
    ap.add_argument("--out", default="lv2-sweep", help="where results go")
    ap.add_argument("--jobs", type=int, default=max(1, min(8, (os.cpu_count() or 2) // 2)))
    ap.add_argument("--batch", type=int, default=20, help="plug-ins per worker process")
    ap.add_argument("--timeout", type=float, default=120, help="seconds without progress before a hang")
    ap.add_argument("--filter", help="only the plug-ins whose URI or name matches this regex")
    ap.add_argument("--limit", type=int, help="at most this many plug-ins")
    ap.add_argument("--lv2-path", help="LV2_PATH to sweep; the system directories by default")
    ap.add_argument("--rejudge", action="store_true", help="judge <out>/results.json again, no sweep")
    ap.add_argument("--require-opt-in", action="store_true", help="skip (77) unless SCORE_LV2_SWEEP=1")
    ap.add_argument("--no-progress", dest="progress", action="store_false")
    args = ap.parse_args()

    if args.require_opt_in and os.environ.get("SCORE_LV2_SWEEP") != "1":
        print("skipped: set SCORE_LV2_SWEEP=1 to sweep the LV2 plug-ins of this machine")
        return 77

    args.out = os.path.abspath(args.out)
    os.makedirs(args.out, exist_ok=True)

    if args.rejudge:
        records = json.loads(Path(args.out, "results.json").read_text())
        return 1 if report(args, {r["uri"]: r for r in records}) else 0

    args.worker = os.path.abspath(args.worker)
    args.lv2_path = lv2_path(args.lv2_path)
    plugins = list_plugins(args)
    if args.filter:
        rx = re.compile(args.filter, re.I)
        plugins = [p for p in plugins if rx.search(p["uri"]) or rx.search(p["name"])]
    if args.limit:
        plugins = plugins[: args.limit]
    if not plugins:
        print(f"no LV2 plug-in found in {args.lv2_path}")
        return 77
    print(f"{len(plugins)} plug-ins in {args.lv2_path}, {args.jobs} workers")
    results = run_sweep(args, plugins)
    return 1 if report(args, results) else 0


if __name__ == "__main__":
    sys.exit(main())
