#!/usr/bin/env python3
"""Every VST 2.4 plug-in installed on the machine, through score's host.

test_vst_sweep_worker probes the libraries found in the VST directories, in
batches: each one is identified and played once in a reference host written
from the VST 2.4 interface, then played twice in score's host (a stop between
the two, as the transport does), and its state saved and loaded back as a
document does. Workers run in parallel; a plug-in that crashes or hangs one is
recorded as such, and the worker restarts with the rest of its batch. A plug-in
that crashes the reference host is probed again in score's host alone.

What each plug-in is decides what it is fed and what is expected of it:
  instrument              notes in, sound out
  effect                  a signal in, a signal out
  midi-controlled-effect  an effect that also takes MIDI
  generator               sound out with nothing in
  analyzer                a signal in, nothing out (its display is its output)
  midi-effect             notes in, MIDI out
  midi-generator          MIDI out with nothing in
  shell                   a library of several plug-ins (effShellGetNextPlugin)
  not a plug-in           no VST 2 entry point, or one that makes no AEffect
Severities:
  host     score's host is the suspect: the reference host does better, the
           second play differs from the first, or score's code crashes or
           hangs where the reference host did not. These fail the ctest entry.
  bug      the plug-in misbehaves in both hosts: crash, hang, NaN
  warn     silent or unresponsive where it should not be, too loud, too slow
  info     worth knowing, often fine: bit-exact at defaults, not a plug-in

Results: <out>/results.json (every record and verdict), <out>/report.md.
"""

import argparse
import json
import math
import os
import re
import shutil
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
    DEFAULT_VST_PATHS = ["~/Library/Audio/Plug-Ins/VST", "/Library/Audio/Plug-Ins/VST"]
else:
    DEFAULT_VST_PATHS = [
        "~/.vst",
        "/usr/lib/vst",
        "/usr/local/lib/vst",
        "/usr/lib64/vst",
        "/usr/local/lib64/vst",
    ]

# The phases the worker goes through, in order. A crash or a hang in one of
# score's phases, after the reference host played the plug-in, points at score.
SCORE_PHASES = ("model", "play", "replay", "state", "teardown")


def vst_paths(arg):
    if arg:
        return [p for p in arg.split(":") if p]
    paths = []
    if os.environ.get("VST_PATH"):
        paths += os.environ["VST_PATH"].split(":")
    paths += [os.path.expanduser(p) for p in DEFAULT_VST_PATHS]
    return [p for p in paths if os.path.isdir(p)]


def list_plugins(paths):
    """Every .so below the directories (.vst bundles on macOS), symbolic links
    followed, each once"""
    seen = set()
    plugins = []
    for root in paths:
        for dirpath, dirnames, filenames in os.walk(root, followlinks=True):
            dirnames.sort()
            if sys.platform == "darwin":
                for d in dirnames:
                    full = os.path.join(dirpath, d)
                    real = os.path.realpath(full)
                    if d.endswith(".vst") and real not in seen:
                        seen.add(real)
                        plugins.append({"path": full, "name": os.path.splitext(d)[0]})
                # A bundle's own contents are not more plug-ins
                dirnames[:] = [d for d in dirnames if not d.endswith(".vst")]
                continue
            for f in sorted(filenames):
                if not f.endswith(".so"):
                    continue
                full = os.path.join(dirpath, f)
                real = os.path.realpath(full)
                if real in seen or not os.path.isfile(real):
                    continue
                seen.add(real)
                plugins.append({"path": full, "name": os.path.splitext(f)[0]})
    return plugins


class Display:
    """A private X server for the plug-ins that open a display connection
    while they load: never the user's"""

    def __init__(self, mode):
        self.proc = None
        self.name = None
        if mode == "none" or not shutil.which("Xvfb"):
            return
        for n in range(120, 200):
            if os.path.exists(f"/tmp/.X11-unix/X{n}") or os.path.exists(f"/tmp/.X{n}-lock"):
                continue
            proc = subprocess.Popen(
                ["Xvfb", f":{n}", "-screen", "0", "1280x800x24", "+extension", "GLX",
                 "+render", "-noreset"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            for _ in range(50):
                if os.path.exists(f"/tmp/.X11-unix/X{n}") or proc.poll() is not None:
                    break
                time.sleep(0.1)
            if proc.poll() is None:
                self.proc = proc
                self.name = f":{n}"
                return

    def close(self):
        if self.proc:
            self.proc.terminate()
            try:
                self.proc.wait(5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
            self.proc = None


def worker_env(args, scratch, extra):
    env = dict(os.environ)
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    env.update(
        {
            "SCORE_DISABLE_AUDIOPLUGINS": "1",
            "SCORE_AUDIO_BACKEND": "dummy",
            "QT_QPA_PLATFORM": "offscreen",
            "TMPDIR": scratch,
            "XDG_CONFIG_HOME": os.path.join(scratch, "config"),
        }
    )
    if args.display_name:
        env["DISPLAY"] = args.display_name
    env.update(extra)
    return env


def job_line(p):
    return p["path"] + ("\tnoref" if p.get("noref") else "") + "\n"


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
        self.jobs.write_text("".join(job_line(p) for p in batch), encoding="utf-8")
        self.results.write_text("")
        self.logf = open(self.log, "wb")
        self.proc = subprocess.Popen(
            [self.args.worker],
            cwd=self.args.build,
            env=worker_env(
                self.args,
                self.scratch,
                {
                    "SCORE_VST_SWEEP_JOBS": str(self.jobs),
                    "SCORE_VST_SWEEP_RESULTS": str(self.results),
                },
            ),
            stdout=self.logf,
            stderr=subprocess.STDOUT,
            stdin=subprocess.DEVNULL,
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


def run_sweep(args, plugins):
    queue = list(plugins)
    slots = [Slot(args, i) for i in range(args.jobs)]
    results = {}
    total = len(plugins)
    started = time.monotonic()
    by_path = {p["path"]: p for p in plugins}

    def finish(rec):
        meta = by_path.get(rec["path"], {})
        prev = results.get(rec["path"])
        rec = {**{k: v for k, v in meta.items() if k != "noref"}, **rec}
        if prev and prev.get("status") in ("crash", "hang") and prev.get("phase") == "reference":
            # The second probe, without the reference host: keep why
            rec["ref_status"] = prev["status"]
            rec["ref_exit"] = prev.get("exit")
            rec["ref_log"] = prev.get("log")
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
                culprit = s.current or rest[0]["path"]
                status = "hang" if hung else "crash"
                finish(
                    {
                        "type": "result",
                        "path": culprit,
                        "status": status,
                        "phase": s.phase,
                        "exit": "timeout" if hung else describe_exit(code),
                        "log": s.log_tail(),
                        "noref": by_path.get(culprit, {}).get("noref", False),
                    }
                )
                requeue = [p for p in rest if p["path"] != culprit]
                if s.phase == "reference" and not by_path[culprit].get("noref"):
                    # Score's host has not run it yet: once more, without the
                    # reference host
                    by_path[culprit] = {**by_path[culprit], "noref": True}
                    requeue.append(by_path[culprit])
                queue[:0] = requeue
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


def backtrace(args, rec):
    """The crash or hang of one plug-in again, alone, under gdb"""
    gdb = shutil.which("gdb")
    if not gdb:
        return None
    with tempfile.TemporaryDirectory(dir=args.out) as scratch:
        jobs = Path(scratch, "jobs.tsv")
        jobs.write_text(job_line(rec), encoding="utf-8")
        results = Path(scratch, "results.jsonl")
        results.write_text("")
        cmd = [gdb, "-batch", "-nx", "-ex", "set pagination off",
               "-ex", "set print thread-events off", "-ex", "handle SIGPIPE nostop noprint",
               "-ex", "run", "-ex", "bt 40", "-ex", "thread apply all bt 12", "--args", args.worker]
        env = worker_env(
            args, scratch,
            {"SCORE_VST_SWEEP_JOBS": str(jobs), "SCORE_VST_SWEEP_RESULTS": str(results)})
        try:
            if rec.get("status") == "hang":
                # Interrupt it once the hang is certain
                proc = subprocess.Popen(
                    cmd, cwd=args.build, env=env, stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, start_new_session=True)
                time.sleep(min(args.timeout, 60) + 30)
                subprocess.run(["pkill", "-INT", "-P", str(proc.pid)], check=False)
                out, _ = proc.communicate(timeout=120)
            else:
                out = subprocess.run(
                    cmd, cwd=args.build, env=env, stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                    timeout=args.timeout * 4).stdout
        except subprocess.TimeoutExpired:
            return "gdb timed out"
        text = out.decode(errors="replace")
        i = text.find("received signal")
        if i >= 0:
            # The thread that crashed first, then the others
            return text[i:][:16000]
        if i < 0:
            i = text.find("Thread 1")
        return text[max(0, i - 200):][-12000:] if i >= 0 else text[-4000:]


def db(a, b):
    if a <= 0 or b <= 0:
        return math.inf
    return 20 * math.log10(a / b)


def judge(r):
    """The verdicts on one record: (severity, code, message)"""
    v = []
    add = lambda sev, code, msg: v.append({"severity": sev, "code": code, "message": msg})
    status = r.get("status")
    if status in ("crash", "hang"):
        where = r.get("phase") or "?"
        what = f"crashed during {where} ({r.get('exit')})" if status == "crash" else f"no progress for the timeout during {where}"
        if where in SCORE_PHASES and r.get("ref_status") is None and not r.get("noref"):
            add("host", status, what + "; the reference host played it")
        elif where in SCORE_PHASES:
            add("bug", status, what + f"; the reference host {r.get('ref_status', 'was skipped')} too")
        else:
            add("bug", status, what)
        return v
    if status == "not-a-plugin":
        add("info", "not-a-plugin", r.get("reason", ""))
        return v
    if status == "shell":
        kids = r.get("shell_children") or []
        add("warn", "shell", f"a shell of {len(kids)} plug-ins, which score's host cannot open one by one")
        return v
    if status == "instantiate-failed":
        add("host", "instantiate-failed", "score's host made no instance, the reference host did")
        return v

    if r.get("ref_status"):
        add("bug", "ref-" + r["ref_status"], f"the reference host {r['ref_status']}ed during its play ({r.get('ref_exit')}): score's host played it alone")

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
        add("host", "nonfinite-on-replay", "NaN or infinite samples only on the second play")

    if has_audio_out and role in ("instrument", "effect", "midi-controlled-effect", "generator"):
        if play_peak < SILENT:
            if ref_ran and ref_peak >= SILENT:
                add("host", "silent-in-score", f"silent in score, peak {ref_peak:.3g} in the reference host")
            else:
                why = {
                    "instrument": "silent while notes are held",
                    "effect": "silent while fed a signal",
                    "midi-controlled-effect": "silent while fed a signal",
                    "generator": "silent with nothing to drive it",
                }[role]
                add("warn", "silent", why + " in both hosts")
        elif replay_peak < SILENT:
            add("host", "silent-on-replay", f"sound on the first play (peak {play_peak:.3g}), silent on the second")
        elif ref_ran and ref_peak >= SILENT:
            diff = db(r.get("play_rms", 0), r.get("ref_rms", 0))
            if abs(diff) > UNSTABLE_DB and max(play_peak, ref_peak) > LOUD:
                add("warn", "unstable", f"peak {play_peak:.3g} in score, {ref_peak:.3g} in the reference host: uninitialised state?")
            elif abs(diff) > LEVEL_MISMATCH_DB:
                add("host", "level-mismatch", f"{diff:+.1f} dB from the reference host")
        if role == "instrument" and play_peak >= SILENT and (r.get("play_peak_tail") or 0) >= 0.5 * play_peak and (r.get("play_rms_held") or 0) > 0:
            add("info", "no-release", "as loud at the end as while notes were held: notes may hang")

    if has_audio_out and play_peak > LOUD:
        add("warn", "loud", f"peak {play_peak:.3g} ({20 * math.log10(play_peak):+.1f} dBFS)")
    rms = r.get("play_rms", 0) or 0
    dc = r.get("play_dc", 0) or 0
    if has_audio_out and abs(dc) > 0.05 and abs(dc) > 0.5 * rms:
        add("warn", "dc", f"DC offset {dc:+.3f}")
    if role in ("effect", "midi-controlled-effect") and r.get("play_max_diff_input") is not None and r["play_max_diff_input"] < 1e-7 and not play_nf and play_peak >= SILENT:
        add("info", "passthrough", "bit-exact passthrough at its default settings")

    if role == "midi-controlled-effect" and not r.get("score_midi_in"):
        add("host", "no-midi-input", "takes MIDI (canDo receiveVstMidiEvent) but score's host gives it no MIDI input")
    if role in ("midi-effect", "midi-generator"):
        ref_out, play_out = r.get("ref_midi_out") or 0, r.get("play_midi_out") or 0
        if ref_out > 0 and play_out == 0:
            add("host", "midi-output-dropped", f"{ref_out} MIDI events out in the reference host, none in score's host")
        elif ref_out == 0 and play_out == 0 and role == "midi-effect":
            add("warn", "no-midi", "notes in, no MIDI out in the reference host either")
        if not r.get("score_midi_in") and role == "midi-effect":
            add("host", "no-midi-input", "takes MIDI but score's host gives it no MIDI input")

    if r.get("state_loaded") is False:
        add("host", "state-not-loaded", "the saved process loaded back without a plug-in")
    if (r.get("state_param_mismatch") or 0) > 0:
        ref = r.get("ref_state_param_mismatch")
        if ref == 0:
            add("host", "state-mismatch", f"{r['state_param_mismatch']} parameters differ after a save and a load, none in the reference host")
        else:
            add("warn", "state-mismatch", f"{r['state_param_mismatch']} parameters differ after a save and a load ({ref} in the reference host)")

    rt = r.get("play_rt_factor") or 0
    if rt > 1.0:
        add("warn", "slow", f"{rt:.2f}x real time in score's host")
    if (r.get("instantiate_ms") or 0) > 5000:
        add("info", "slow-load", f"{r['instantiate_ms'] / 1000:.1f} s to instantiate")
    return v


def mark_duplicate_ids(records):
    """score finds a VST 2 plug-in by its unique ID: two libraries with the same
    one cannot both be used"""
    by_id = defaultdict(list)
    for r in records:
        if r.get("status") == "ran" or r.get("unique_id") is not None:
            if r.get("unique_id") is not None:
                by_id[r["unique_id"]].append(r)
    for uid, rs in by_id.items():
        if len(rs) > 1:
            names = ", ".join(sorted(x.get("name", "?") for x in rs))
            for r in rs:
                r.setdefault("extra_verdicts", []).append(
                    {"severity": "warn", "code": "duplicate-id",
                     "message": f"unique ID {uid} shared by {names}: score opens only one of them"})


SEVERITIES = ["host", "bug", "warn", "info"]


def report(args, results):
    records = sorted(results.values(), key=lambda r: (r.get("name") or r["path"]).lower())
    mark_duplicate_ids(records)
    for r in records:
        r["verdicts"] = judge(r) + r.pop("extra_verdicts", [])
    Path(args.out, "results.json").write_text(json.dumps(records, indent=1))

    def category(r):
        s = r.get("status")
        if s == "ran":
            return r.get("role", "other")
        if s == "shell":
            return "shell"
        if s == "not-a-plugin":
            return "not a plug-in"
        return r.get("role") or s
    roles = Counter(category(r) for r in records)
    sev_count = Counter()
    by_code = defaultdict(list)
    for r in records:
        for v in r["verdicts"]:
            sev_count[v["severity"]] += 1
            by_code[(v["severity"], v["code"])].append((r, v))
    clean = sum(1 for r in records if not any(v["severity"] != "info" for v in r["verdicts"]))

    lines = ["# VST 2.4 system sweep", ""]
    lines.append(f"{len(records)} libraries, {clean} without a host, bug or warn verdict.")
    lines.append("")
    lines.append("| category | libraries |")
    lines.append("|---|---|")
    for role, n in roles.most_common():
        lines.append(f"| {role} | {n} |")
    lines.append("")
    lines.append("| severity | verdicts |")
    lines.append("|---|---|")
    for sev in SEVERITIES:
        lines.append(f"| {sev} | {sev_count[sev]} |")
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
                lines.append(f"- **{r.get('name')}** `{r['path']}` ({category(r)}): {v['message']}")
                if r.get("backtrace") and v["code"] in ("crash", "hang"):
                    lines.append("")
                    lines.append("  ```")
                    for l in r["backtrace"].splitlines()[:40]:
                        lines.append("  " + l)
                    lines.append("  ```")
            lines.append("")
    Path(args.out, "report.md").write_text("\n".join(lines))

    print(f"{len(records)} libraries; " + ", ".join(f"{s}: {sev_count[s]}" for s in SEVERITIES))
    for sev in ("host", "bug"):
        for k in sorted(k for k in by_code if k[0] == sev):
            for r, v in by_code[k]:
                print(f"  [{sev}] {r.get('name')}: {v['message']}")
    print(f"report: {Path(args.out, 'report.md')}")
    return sev_count["host"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--worker", required=True, help="the test_vst_sweep_worker executable")
    ap.add_argument("--build", default=os.getcwd(), help="score's build directory, where workers run")
    ap.add_argument("--out", default="vst-sweep", help="where results go")
    ap.add_argument("--jobs", type=int, default=max(1, min(4, (os.cpu_count() or 2) // 4)))
    ap.add_argument("--batch", type=int, default=10, help="libraries per worker process")
    ap.add_argument("--timeout", type=float, default=120, help="seconds without progress before a hang")
    ap.add_argument("--filter", help="only the libraries whose path matches this regex")
    ap.add_argument("--exclude", help="leave out the libraries whose path matches this regex")
    ap.add_argument("--limit", type=int, help="at most this many libraries")
    ap.add_argument("--vst-path", help="colon-separated directories to sweep; VST_PATH and the system ones by default")
    ap.add_argument("--display", choices=["xvfb", "none"], default="xvfb",
                    help="a private Xvfb for the plug-ins that need a display (default), or no display at all")
    ap.add_argument("--backtrace", action="store_true", help="run each crash and hang again under gdb")
    ap.add_argument("--rejudge", action="store_true", help="judge <out>/results.json again, no sweep")
    ap.add_argument("--require-opt-in", action="store_true", help="skip (77) unless SCORE_VST_SWEEP=1")
    ap.add_argument("--no-progress", dest="progress", action="store_false")
    args = ap.parse_args()

    if args.require_opt_in and os.environ.get("SCORE_VST_SWEEP") != "1":
        print("skipped: set SCORE_VST_SWEEP=1 to sweep the VST 2 plug-ins of this machine")
        return 77

    args.out = os.path.abspath(args.out)
    os.makedirs(args.out, exist_ok=True)

    if args.rejudge:
        records = json.loads(Path(args.out, "results.json").read_text())
        return 1 if report(args, {r["path"]: r for r in records}) else 0

    args.worker = os.path.abspath(args.worker)
    paths = vst_paths(args.vst_path)
    plugins = list_plugins(paths)
    if args.filter:
        rx = re.compile(args.filter, re.I)
        plugins = [p for p in plugins if rx.search(p["path"])]
    if args.exclude:
        rx = re.compile(args.exclude, re.I)
        plugins = [p for p in plugins if not rx.search(p["path"])]
    if args.limit:
        plugins = plugins[: args.limit]
    if not plugins:
        print(f"no VST library found in {':'.join(paths)}")
        return 77

    display = Display(args.display)
    args.display_name = display.name
    try:
        print(f"{len(plugins)} libraries in {':'.join(paths)}, {args.jobs} workers, display {display.name or 'none'}")
        results = run_sweep(args, plugins)
        if args.backtrace:
            for r in results.values():
                if r.get("status") in ("crash", "hang"):
                    r["backtrace"] = backtrace(args, r)
    finally:
        display.close()
    return 1 if report(args, results) else 0


if __name__ == "__main__":
    sys.exit(main())
