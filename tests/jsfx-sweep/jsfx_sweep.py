#!/usr/bin/env python3
"""Every JSFX effect installed on the machine, through score's host.

The files of the JSFX folders are sorted first: an effect has a desc: line in
its header; .jsfx-inc files and header-less code are libraries that effects
import; the rest (images, samples, presets, text) is data. Only effects are
probed.

test_jsfx_sweep_worker probes them in batches: each one is loaded as a YSFX
process of a document, plays twice in score's host (stopped between the two,
as the transport does), has its sliders driven through their ports and their
controls, plays with every block cut in two ticks, is saved and loaded again,
and plays once in the ysfx library used directly (the reference). Workers run
in parallel; an effect that crashes or hangs one is recorded as such, and the
worker restarts with the rest of its batch.

What an effect is decides what is expected of it (every one gets a signal and
notes):
  instrument      reads MIDI, no audio input, sound out
  effect          audio in, audio out
  generator       sound out with nothing in
  midi-effect     reads and writes MIDI
  analyzer        audio in, no audio out
Severities:
  host     score's host is the suspect: the reference does better, the controls
           do not reach the sliders, a save loses something, or the second play
           differs from the first. These fail the ctest entry.
  bug      the effect misbehaves in both hosts: crash, hang, NaN
  warn     silent where it should not be, too loud, too slow
  info     worth knowing, often fine: compiles nowhere, bit-exact at defaults

Results: <out>/results.json (every record and verdict), <out>/report.md.

With --live, each effect is instead added to a playing document in every way
a user can: see tests/tools/live_phases.py.
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

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import live_phases  # noqa: E402

SILENT = 1e-5  # -100 dBFS
LOUD = 4.0  # +12 dBFS
LEVEL_MISMATCH_DB = 12.0

if sys.platform == "darwin":
    DEFAULT_ROOTS = ["~/Library/Application Support/REAPER/Effects", "~/Documents/jsfx"]
else:
    DEFAULT_ROOTS = ["~/.config/REAPER/Effects", "~/Documents/jsfx"]
SKIP_DIRS = {".git", ".github", "__pycache__"}
DATA_EXTENSIONS = {
    "png", "jpg", "jpeg", "gif", "bmp", "svg", "wav", "flac", "ogg", "mp3", "aif", "aiff",
    "txt", "md", "rpl", "xml", "html", "htm", "pdf", "ini", "json", "py", "lua", "eel",
    "zip", "gz", "sh", "bat", "reabank", "reascale", "rpp", "ttf", "otf", "csv", "yml",
    "yaml", "sample", "pack", "idx", "rev", "gitattributes", "gitignore", "gitmodules",
}
DESC = re.compile(rb"^\s*desc:", re.M)
CODE = re.compile(rb"^\s*(@init|@sample|@block|@slider|@serialize|@gfx|function\s)", re.M)


def classify(path):
    """effect, library or data, and why"""
    ext = path.suffix.lower().lstrip(".")
    if ext in DATA_EXTENSIONS or path.name.startswith("."):
        return "data", ext or "dotfile"
    try:
        with open(path, "rb") as f:
            head = f.read(1 << 20)
    except OSError as e:
        return "data", f"unreadable: {e}"
    if b"\0" in head[:4096]:
        return "data", "binary"
    if ext == "jsfx-inc":
        return "library", "jsfx-inc"
    # The header ends with the first section
    first = re.search(rb"^\s*@", head, re.M)
    header = head[: first.start()] if first else head
    if DESC.search(header):
        return "effect", ext or "no extension"
    if CODE.search(head):
        return "library", "code without desc:"
    return "data", "text without desc: or code"


def discover(roots):
    files = []
    for root in roots:
        root = Path(os.path.expanduser(root))
        if not root.is_dir():
            continue
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS)
            for name in sorted(filenames):
                p = Path(dirpath) / name
                if not p.is_file():
                    continue
                kind, why = classify(p)
                files.append({"path": str(p), "root": str(root), "kind": kind, "why": why})
    return files


def interleave(effects):
    """Effects of the different roots one after the other, as a session loading
    effects of several folders does: each must find the files of its own tree,
    whichever were loaded before it"""
    by_root = defaultdict(list)
    for e in effects:
        by_root[e["root"]].append(e)
    lists = list(by_root.values())
    out = []
    while any(lists):
        for l in lists:
            if l:
                out.append(l.pop(0))
    return out


def worker_env(scratch, extra):
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
        self.jobs = base / "jobs.txt"
        self.results = base / "results.jsonl"
        self.log = base / "worker.log"
        self.jobs.write_text("".join(f"{p['path']}\n" for p in batch), encoding="utf-8")
        self.results.write_text("")
        self.logf = open(self.log, "wb")
        self.proc = subprocess.Popen(
            [self.args.worker],
            cwd=self.args.build,
            env=worker_env(
                self.scratch,
                {
                    "SCORE_JSFX_SWEEP_JOBS": str(self.jobs),
                    "SCORE_JSFX_SWEEP_RESULTS": str(self.results),
                    **live_phases.worker_env(self.args),
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
                self.current = rec["path"]
                self.phase = rec["phase"]
            elif rec.get("type") == "result":
                self.done.add(rec["path"])
                self.current = None
                out.append(rec)
        return out

    def remaining(self):
        return [p for p in self.batch if p["path"] not in self.done]

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
        return "\n".join(lines[-n:])


def describe_exit(code):
    if code < 0:
        try:
            import signal

            return f"signal {signal.Signals(-code).name}"
        except (ValueError, ImportError):
            return f"signal {-code}"
    return f"exit code {code}"


def run_sweep(args, effects):
    queue = list(effects)
    slots = [Slot(args, i) for i in range(args.jobs)]
    results = {}
    total = len(effects)
    started = time.monotonic()
    by_path = {p["path"]: p for p in effects}

    def finish(rec):
        rec = {**by_path.get(rec["path"], {}), **rec}
        results[rec["path"]] = rec

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
                for o in slots:
                    if o.proc and o is not s:
                        o.kill()
                raise SystemExit(
                    f"the worker failed before probing anything ({describe_exit(code)}):\n"
                    + s.log_tail(40)
                )
            if rest:
                culprit = s.current or rest[0]["path"]
                finish(
                    {
                        "type": "result",
                        "path": culprit,
                        "status": "hang" if hung else "crash",
                        "phase": s.phase,
                        "exit": "timeout" if hung else describe_exit(code),
                        "log": s.log_tail(),
                    }
                )
                queue[:0] = [p for p in rest if p["path"] != culprit]
            s.proc = None
            if queue:
                s.start(next_batch())
        if args.progress:
            sys.stderr.write(f"\r{len(results)}/{total} effects, {time.monotonic() - started:.0f} s   ")
            sys.stderr.flush()
    if args.progress:
        sys.stderr.write("\n")
    return results


def db(a, b):
    a, b = a or 0, b or 0
    if a <= 0 or b <= 0:
        return math.inf
    return 20 * math.log10(a / b)


def role_of(r):
    try:
        text = Path(r["path"]).read_bytes()
    except OSError:
        text = b""
    reads_midi = re.search(rb"\bmidirecv(_buf|_str)?\s*\(", text) is not None
    writes_midi = re.search(rb"\bmidisend(_buf|_str)?\s*\(", text) is not None
    header = text.split(b"\n@", 1)[0].lower()
    synth = re.search(rb"^\s*(desc|tags):.*\b(synth|instrument|sampler)", header, re.M) is not None
    ins, outs = r.get("inputs", 0) or 0, r.get("outputs", 0) or 0
    if reads_midi and writes_midi and not (outs > 0 and synth):
        return "midi-effect"
    if outs > 0 and reads_midi and (ins == 0 or synth):
        return "instrument"
    if ins > 0 and outs > 0:
        return "effect"
    if outs > 0:
        return "generator"
    if ins > 0:
        return "analyzer"
    return "midi-effect" if writes_midi else "other"


def judge(r):
    """The verdicts on one record: (severity, code, message)"""
    if live_phases.is_live(r):
        return live_phases.judge(r)
    v = []
    add = lambda sev, code, msg: v.append({"severity": sev, "code": code, "message": msg})
    status = r.get("status")
    where = r.get("phase") or "?"
    if status in ("crash", "hang"):
        # Phases in score's code; the reference ones run the library alone
        sev = "info" if where in ("reference", "reference-play") else "host"
        what = "crashed" if status == "crash" else "no progress for the timeout"
        add(sev, status, f"{what} during {where} ({r.get('exit')})")
        return v
    if status == "load-failed":
        add("info", "load-failed", "ysfx cannot load it: " + "; ".join(r.get("ref_errors") or []))
        return v
    if status == "compile-failed":
        add("info", "compile-failed", "does not compile in the reference either: " + "; ".join(r.get("ref_errors") or [])[:300])
        return v
    if status == "score-compile-failed":
        add("host", "score-compile-failed", "compiles in the reference, not in score "
            f"(import root {r.get('score_import_root')!r} in score, {r.get('ref_import_root')!r} in the reference)")
        return v
    if status in ("no-document", "no-node", "no-node-on-replay"):
        add("host", status, "score's host made no " + ("document" if status == "no-document" else "node"))
        return v

    for code, key, msg in (
        ("control-mismatch", "control_mismatch", "controls do not match the sliders"),
        ("port-mismatch", "port_mismatch", "a slider's port drives another slider"),
        ("ui-mismatch", "ui_mismatch", "a control does not reach its slider"),
        ("reload-state", "reload_state_mismatch", "a save and load loses slider values"),
        ("reload-ui-mismatch", "reload_ui_mismatch", "after a load, a control does not reach its slider"),
    ):
        items = r.get(key) or []
        if items:
            add("host", code, f"{msg}: " + "; ".join(items[:4]) + (f" (+{len(items) - 4})" if len(items) > 4 else ""))

    if r.get("reload_found") is False:
        add("host", "reload-lost", "the process is gone after a save and load")
    elif r.get("reload_compiled") is False:
        add("host", "reload-compile", "does not compile after a save and load")
    bp, bps, bpr = r.get("bank_presets"), r.get("bank_presets_score"), r.get("bank_presets_reload")
    if bp and bps is not None and bps < bp:
        add("host", "presets-missing", f"{bp} presets in its bank, {bps:.0f} in score")
    elif bp and bpr is not None and bpr < (bps or 0):
        add("host", "presets-lost-on-reload", f"{bps:.0f} presets, {bpr:.0f} after a save and load")

    rate, block = r.get("rate"), r.get("block")
    if rate and r.get("score_srate") is not None and abs(r["score_srate"] - rate) > 0.5:
        add("host", "srate", f"srate {r['score_srate']} in the effect, {rate} in score")
    if block and r.get("score_samplesblock") is not None and abs(r["score_samplesblock"] - block) > 0.5:
        add("host", "samplesblock", f"samplesblock {r['score_samplesblock']} in the effect, {block} in score")

    role = r.get("role", "other")
    play_peak = r.get("play_peak") or 0
    replay_peak = r.get("replay_peak") or 0
    ref_peak = r.get("ref_peak") or 0
    has_out = (r.get("outputs") or 0) > 0

    if (r.get("play_nonfinite") or 0) > 0:
        if (r.get("ref_nonfinite") or 0) == 0:
            add("host", "nonfinite", "NaN or infinite samples, none in the reference")
        else:
            add("bug", "nonfinite", "NaN or infinite samples in both hosts")
    elif (r.get("replay_nonfinite") or 0) > 0:
        add("host", "nonfinite-after-stop", "NaN or infinite samples only on the second play")

    if has_out:
        if play_peak < SILENT:
            if ref_peak >= SILENT:
                add("host", "silent-in-score", f"silent in score, peak {ref_peak:.3g} in the reference")
            elif role in ("instrument", "effect", "generator"):
                add("warn", "silent", f"silent in both hosts while fed a signal and notes ({role})")
        else:
            if replay_peak < SILENT and not (r.get("play_nonfinite") or 0):
                add("host", "silent-on-replay", f"sound on the first play (peak {play_peak:.3g}), silent on the second")
            if r.get("reload_peak") is not None and (r.get("reload_peak") or 0) < SILENT and replay_peak >= SILENT:
                add("host", "silent-after-reload", f"sound before a save and load (peak {replay_peak:.3g} on the same length), silent after")
            if ref_peak >= SILENT:
                diff = db(r.get("play_rms", 0), r.get("ref_rms", 0))
                if abs(diff) > LEVEL_MISMATCH_DB:
                    add("info", "level-mismatch", f"{diff:+.1f} dB from the reference")
        sb, sfs = r.get("split_split_blocks") or 0, r.get("split_split_first_silent") or 0
        if sb >= 8 and sfs >= 0.5 * sb:
            add("host", "split-tick-silenced", f"blocks cut in two ticks: the first part is silent in {sfs:.0f} of {sb:.0f}")
        if (r.get("play_out_channels") or 0) != (r.get("ref_out_channels") or 0):
            add("host", "channels", f"{r.get('play_out_channels')} output channels in score, {r.get('ref_out_channels')} in the reference")
        if play_peak > LOUD:
            sev = "warn" if ref_peak > LOUD else "host"
            add(sev, "loud", f"peak {play_peak:.3g} in score, {ref_peak:.3g} in the reference")
        if role == "effect" and r.get("play_max_diff_input") is not None and r["play_max_diff_input"] < 1e-7:
            add("info", "passthrough", "bit-exact passthrough at its default settings")

    pm, rm = r.get("play_midi_out") or 0, r.get("ref_midi_out") or 0
    if rm > 0 and pm == 0:
        add("host", "no-midi-in-score", f"no MIDI out in score, {rm} events in the reference")
    if role == "midi-effect" and pm == 0 and rm == 0:
        add("warn", "no-midi", "notes in, no MIDI out in either host")
    sn, rsn = r.get("split_notes_out"), r.get("ref_split_notes_out")
    if sn is not None and rsn is not None and sn > 1.5 * rsn + 1:
        add("host", "split-notes-duplicated", f"{sn:.0f} notes out with blocks cut in two ticks, {rsn:.0f} in the reference cut the same way")

    rt = r.get("play_rt_factor") or 0
    if rt > 1.0:
        add("warn" if (r.get("ref_rt_factor") or 0) > 0.5 else "host", "slow",
            f"{rt:.2f}x real time in score, {r.get('ref_rt_factor', 0):.2f}x in the reference")
    if (r.get("stop_ms") or 0) > 500:
        add("info", "slow-stop", f"{r['stop_ms']:.0f} ms to stop")
    if (r.get("load_ms") or 0) > 5000:
        add("info", "slow-load", f"{r['load_ms'] / 1000:.1f} s to load")
    if r.get("enum_default_outside"):
        add("info", "default-outside-choices", "a choice slider's default is not one of its choices, which ysfx counts from 0: " + ", ".join(r["enum_default_outside"]))
    if (r.get("pdc_delay") or 0) > 0:
        add("info", "pdc", f"reports {r['pdc_delay']:.0f} samples of latency")
    return v


SEVERITIES = ["host", "bug", "warn", "info"]


def report(args, results, files):
    records = sorted(results.values(), key=lambda r: r["path"].lower())
    for r in records:
        if r.get("status") == "ran" or "inputs" in r:
            r["role"] = role_of(r)
        r["verdicts"] = judge(r)
    Path(args.out, "results.json").write_text(json.dumps({"files": files, "effects": records}, indent=1))

    kinds = Counter(f["kind"] for f in files)
    statuses = Counter(r.get("status") for r in records)
    roles = Counter(r.get("role", r.get("status")) for r in records)
    sev_count = Counter()
    by_code = defaultdict(list)
    for r in records:
        for v in r["verdicts"]:
            sev_count[v["severity"]] += 1
            by_code[(v["severity"], v["code"])].append((r, v))
    clean = sum(1 for r in records if not any(v["severity"] != "info" for v in r["verdicts"]))

    lines = ["# JSFX system sweep", ""]
    lines.append(f"{len(files)} files: " + ", ".join(f"{n} {k}" for k, n in kinds.most_common()) + ".")
    lines.append(f"{len(records)} effects probed, {clean} without a host, bug or warn verdict.")
    lines.append("")
    lines.append("| status | effects |")
    lines.append("|---|---|")
    for k, n in statuses.most_common():
        lines.append(f"| {k} | {n} |")
    lines.append("")
    lines.append("| role | effects |")
    lines.append("|---|---|")
    for role, n in roles.most_common():
        lines.append(f"| {role} | {n} |")
    lines.append("")
    lines.append("| severity | verdicts |")
    lines.append("|---|---|")
    for s in SEVERITIES:
        lines.append(f"| {s} | {sev_count[s]} |")
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
                lines.append(f"- **{r.get('name') or Path(r['path']).name}** `{r['path']}` ({r.get('role', '?')}): {v['message']}")
            lines.append("")
    lines += live_phases.report_lines(records, lambda r: r.get("name") or Path(r["path"]).name)
    lines.append("## files that are not effects")
    lines.append("")
    why = Counter((f["kind"], f["why"]) for f in files if f["kind"] != "effect")
    for (k, w), n in sorted(why.items(), key=lambda x: -x[1]):
        lines.append(f"- {k}, {w}: {n}")
    lines.append("")
    Path(args.out, "report.md").write_text("\n".join(lines))

    print(f"{len(records)} effects; " + ", ".join(f"{s}: {sev_count[s]}" for s in SEVERITIES))
    for sev in ("host", "bug"):
        for k in sorted(k for k in by_code if k[0] == sev):
            for r, v in by_code[k]:
                print(f"  [{sev}] {r.get('name') or r['path']}: {v['message']}")
    print(f"report: {Path(args.out, 'report.md')}")
    return sev_count["host"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--worker", help="the test_jsfx_sweep_worker executable")
    ap.add_argument("--build", default=os.getcwd(), help="score's build directory, where workers run")
    ap.add_argument("--out", default="jsfx-sweep", help="where results go")
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--batch", type=int, default=20, help="effects per worker process")
    ap.add_argument("--timeout", type=float, default=120, help="seconds without progress before a hang")
    ap.add_argument("--filter", help="only the effects whose path matches this regex")
    ap.add_argument("--limit", type=int, help="at most this many effects")
    ap.add_argument("--root", action="append", help="a JSFX folder to sweep (repeatable); "
                    + ", ".join(DEFAULT_ROOTS) + " by default")
    ap.add_argument("--rejudge", action="store_true", help="judge <out>/results.json again, no sweep")
    ap.add_argument("--require-opt-in", action="store_true", help="skip (77) unless SCORE_JSFX_SWEEP=1")
    ap.add_argument("--no-progress", dest="progress", action="store_false")
    live_phases.add_arguments(ap)
    args = ap.parse_args()

    if args.require_opt_in and os.environ.get("SCORE_JSFX_SWEEP") != "1":
        print("skipped: set SCORE_JSFX_SWEEP=1 to sweep the JSFX effects of this machine")
        return 77

    args.out = os.path.abspath(args.out)
    os.makedirs(args.out, exist_ok=True)

    if args.rejudge:
        data = json.loads(Path(args.out, "results.json").read_text())
        return 1 if report(args, {r["path"]: r for r in data["effects"]}, data["files"]) else 0

    if not args.worker:
        ap.error("--worker is required")
    args.worker = os.path.abspath(args.worker)
    files = discover(args.root or DEFAULT_ROOTS)
    effects = interleave([f for f in files if f["kind"] == "effect"])
    if args.filter:
        rx = re.compile(args.filter, re.I)
        effects = [e for e in effects if rx.search(e["path"])]
    if args.limit:
        effects = effects[: args.limit]
    if not effects:
        print("no JSFX effect found in " + ", ".join(args.root or DEFAULT_ROOTS))
        return 77
    print(f"{len(files)} files, {len(effects)} effects, {args.jobs} workers")
    results = run_sweep(args, effects)
    # Kept before judging, so that a verdict can be judged again with --rejudge
    Path(args.out, "results.json").write_text(json.dumps({"files": files, "effects": list(results.values())}, indent=1))
    return 1 if report(args, results, files) else 0


if __name__ == "__main__":
    sys.exit(main())
