#!/usr/bin/env python3
"""The editors of every plug-in installed on the machine, through score's host.

test_plugin_gui_sweep_worker opens each editor (VST 2, VST3, CLAP, LV2, and
the JSFX effects with a @gfx section) with score's own windows, the way the
"show UI" button of a process does, on a private Xvfb started here, never on
the user's display. Per plug-in, in a document of its own, it checks that:

  open       the editor maps a window of a sane size that is not blank
  resize     a resize from outside is followed, when the plug-in says it can
  interact   a drag or a scroll of the pointer (xdotool) over the editor
             changes a control of the process; how many undo steps that made,
             what gestures the plug-in reported (VST3 beginEdit / endEdit,
             CLAP PARAM_GESTURE_BEGIN / END), whether controls were created
  host       a control changed in score reaches the plug-in and its editor
  close      closing removes the window, opening again works
  play       opening and driving the editor while the transport plays
  persist    the change made in the editor survives a save and a reload, and
             a document closes with its editor open

CLAP plug-ins run twice: as score's host is, and with the gestures score's
host sent around its own changes up to a04edef446 (the worker adds them
between score and the plug-in, SCORE_GUI_SWEEP_CLAP_GESTURES=1); the report
compares the two.

One worker at a time, each in a systemd scope of its own with a memory limit
(plug-in editors rendering with llvmpipe can take a lot); a new batch only
starts when enough memory is available. A plug-in that crashes or hangs the
worker is recorded as such, with the step it was in, and the worker restarts
with the rest of its batch.

Results: <out>/results.json, <out>/report.md, screenshots below <out>/shots.
"""

import argparse
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time
from collections import Counter, defaultdict
from pathlib import Path

FORMATS = ("vst", "vst3", "clap", "lv2", "jsfx")
DEFAULT_PATHS = {
    "vst": ["~/.vst", "/usr/lib/vst", "/usr/local/lib/vst"],
    "vst3": ["~/.vst3", "/usr/lib/vst3", "/usr/local/lib/vst3"],
    "clap": ["~/.clap", "/usr/lib/clap", "/usr/local/lib/clap"],
    "jsfx": ["~/.config/REAPER/Effects", "~/Documents/jsfx"],
}
GFX = re.compile(rb"^\s*@gfx", re.M)
DESC = re.compile(rb"^\s*desc:", re.M)

# Screen of the private X server
SCREEN = (1920, 1080)
# A window smaller than this, or larger than the screen, is not a sane size
MIN_SIDE = 32
# Pixels that must change for an editor to have shown a change
MIN_DIFF = 30
# A window, its embedded child and the plug-in's own size agree within this
SIZE_SLACK = 16


def describe(p):
    """What --filter and --exclude match: format|key|name|file"""
    return f"{p['format']}|{p['key']}|{p.get('name', '')}|{p.get('file', '')}"


def expand(paths):
    return [os.path.expanduser(p) for p in paths if os.path.isdir(os.path.expanduser(p))]


def find_files(fmt):
    roots = expand(DEFAULT_PATHS.get(fmt, []))
    seen, out = set(), []
    for root in roots:
        for dirpath, dirnames, filenames in os.walk(root, followlinks=True):
            dirnames.sort()
            if fmt == "vst3":
                bundles = [d for d in dirnames if d.endswith(".vst3")]
                for d in bundles:
                    full = os.path.join(dirpath, d)
                    real = os.path.realpath(full)
                    if real not in seen:
                        seen.add(real)
                        out.append(full)
                # Nothing to find inside a bundle
                dirnames[:] = [d for d in dirnames if not d.endswith(".vst3")]
                continue
            for f in sorted(filenames):
                full = os.path.join(dirpath, f)
                real = os.path.realpath(full)
                if real in seen or not os.path.isfile(real):
                    continue
                if fmt == "vst" and not f.endswith(".so"):
                    continue
                if fmt == "clap" and not f.endswith(".clap"):
                    continue
                if fmt == "jsfx":
                    if f.endswith(".jsfx-inc") or "." in f and f.rsplit(".", 1)[1].lower() in (
                        "png", "jpg", "wav", "txt", "md", "rpl", "ogg", "flac", "json", "xml", "gif"):
                        continue
                    try:
                        with open(real, "rb") as fh:
                            head = fh.read(1 << 20)
                    except OSError:
                        continue
                    if b"\0" in head[:4096] or not DESC.search(head) or not GFX.search(head):
                        continue
                seen.add(real)
                out.append(full)
    return out


class Display:
    """A private X server, never the user's"""

    def __init__(self, number=None):
        self.proc = None
        self.name = None
        candidates = [number] if number else range(120, 200)
        for n in candidates:
            if os.path.exists(f"/tmp/.X11-unix/X{n}") or os.path.exists(f"/tmp/.X{n}-lock"):
                continue
            proc = subprocess.Popen(
                ["Xvfb", f":{n}", "-screen", "0", f"{SCREEN[0]}x{SCREEN[1]}x24", "+extension",
                 "GLX", "+render", "-noreset"],
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
        raise SystemExit("could not start a private Xvfb")

    def alive(self):
        return self.proc is not None and self.proc.poll() is None

    def close(self):
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()


def mem_available_gb():
    try:
        with open("/proc/meminfo") as f:
            for line in f:
                if line.startswith("MemAvailable:"):
                    return int(line.split()[1]) / (1024 * 1024)
    except OSError:
        pass
    return 1e9


def wait_for_memory(args):
    waited = False
    while mem_available_gb() < args.min_free_gb:
        if not waited:
            sys.stderr.write(f"\nwaiting for {args.min_free_gb} GB of available memory "
                             f"({mem_available_gb():.1f} GB now)\n")
            waited = True
        time.sleep(10)


def worker_env(args, display, scratch, extra):
    env = dict(os.environ)
    for k in ("WAYLAND_DISPLAY", "QT_QPA_PLATFORM"):
        env.pop(k, None)
    env.update(
        {
            "DISPLAY": display.name,
            "QT_QPA_PLATFORM": "xcb",
            "LIBGL_ALWAYS_SOFTWARE": "1",
            "SCORE_DISABLE_AUDIOPLUGINS": "1",
            "SCORE_AUDIO_BACKEND": "dummy",
            "QT_FORCE_STDERR_LOGGING": "1",
            "TMPDIR": scratch,
            "XDG_CONFIG_HOME": os.path.join(scratch, "config"),
            # Plug-ins that keep their settings in the home folder keep them
            # in the real one: only the score side is isolated
        }
    )
    env.update(extra)
    return env


def scoped(args, cmd):
    """The command in a systemd scope of its own, with a memory limit"""
    if args.no_scope or not shutil.which("systemd-run"):
        return cmd
    return ["systemd-run", "--user", "--scope", "-q", "-p", f"MemoryMax={args.mem_max}",
            "-p", "MemorySwapMax=0"] + cmd


def read_records(path, offset):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            f.seek(offset)
            data = f.read()
    except OSError:
        return [], offset
    end = data.rfind("\n")
    if end < 0:
        return [], offset
    offset += len(data[: end + 1].encode("utf-8"))
    out = []
    for line in data[: end + 1].splitlines():
        try:
            out.append(json.loads(line))
        except json.JSONDecodeError:
            pass
    return out, offset


def describe_exit(code):
    if code is None:
        return "timeout"
    if code < 0:
        try:
            return f"signal {signal.Signals(-code).name}"
        except ValueError:
            return f"signal {-code}"
    return f"exit code {code}"


def log_tail(path, n=20):
    try:
        lines = Path(path).read_text(errors="replace").splitlines()
    except OSError:
        return ""
    crash = []
    for i, l in enumerate(lines):
        if l.startswith("CRASH signal"):
            crash = lines[i:]
            break
    rest = [l for l in lines if not l.startswith("CRASH")][-n:]
    return "\n".join(rest + crash[:80])


class Worker:
    """One worker process and what it is doing"""

    runs = 0

    def __init__(self, args, display, extra, lines):
        if Worker.runs == 0:
            # A resumed sweep keeps the logs of the runs before
            work = Path(args.out) / "work"
            done = [int(d.name[3:]) for d in work.glob("run*") if d.name[3:].isdigit()] if work.exists() else []
            Worker.runs = max(done, default=0)
        Worker.runs += 1
        self.args = args
        base = Path(args.out) / "work" / f"run{Worker.runs}"
        base.mkdir(parents=True, exist_ok=True)
        self.base = base
        self.jobs = base / "jobs.tsv"
        self.results = base / "results.jsonl"
        self.log = base / "worker.log"
        self.jobs.write_text("".join(l + "\n" for l in lines), encoding="utf-8")
        self.results.write_text("")
        self.offset = 0
        self.logf = open(self.log, "wb")
        env = worker_env(args, display, str(base), {**extra, "SCORE_GUI_SWEEP_SHOTS": str(Path(args.out) / "shots"),
                                                    "SCORE_GUI_SWEEP_JOBS": str(self.jobs),
                                                    "SCORE_GUI_SWEEP_RESULTS": str(self.results)})
        self.proc = subprocess.Popen(
            scoped(args, [args.worker]),
            cwd=args.build,
            env=env,
            stdout=self.logf,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
        self.last_progress = time.monotonic()

    def poll(self):
        recs, self.offset = read_records(self.results, self.offset)
        if recs:
            self.last_progress = time.monotonic()
        return recs

    def kill(self):
        try:
            os.killpg(self.proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        self.proc.wait()

    def close(self):
        self.logf.close()


def list_plugins(args, display):
    """What the worker finds in each file, with the files that crash it
    left out"""
    cache = Path(args.out) / "list.json"
    if cache.exists() and not args.relist:
        return json.loads(cache.read_text())
    lines = []
    for fmt in args.formats:
        if fmt == "lv2":
            lines.append("lv2\t")
        elif fmt != "jsfx":
            lines += [f"{fmt}\t{p}" for p in find_files(fmt)]
    found = []
    for fmt in ("jsfx",):
        if fmt in args.formats:
            for p in find_files("jsfx"):
                found.append({"format": "jsfx", "key": p, "file": p,
                              "name": Path(p).stem})
    failed = []
    while lines:
        wait_for_memory(args)
        base = Path(args.out) / "work" / f"list{len(failed)}"
        base.mkdir(parents=True, exist_ok=True)
        inp, outp, log = base / "input.tsv", base / "list.jsonl", base / "worker.log"
        inp.write_text("".join(l + "\n" for l in lines))
        outp.write_text("")
        with open(log, "wb") as lf:
            proc = subprocess.Popen(
                scoped(args, [args.worker]), cwd=args.build,
                env=worker_env(args, display, str(base), {
                    "SCORE_GUI_SWEEP_LIST": str(outp), "SCORE_GUI_SWEEP_LIST_INPUT": str(inp)}),
                stdout=lf, stderr=subprocess.STDOUT, start_new_session=True)
            last = time.monotonic()
            offset = 0
            current = None
            listed = set()
            while True:
                recs, offset = read_records(outp, offset)
                for r in recs:
                    last = time.monotonic()
                    if r.get("type") == "phase":
                        current = r["file"]
                    elif r.get("type") == "listed":
                        listed.add(r["file"])
                        current = None
                    else:
                        found.append(r)
                code = proc.poll()
                if code is not None:
                    break
                if time.monotonic() - last > args.timeout:
                    os.killpg(proc.pid, signal.SIGKILL)
                    proc.wait()
                    code = None
                    break
                time.sleep(0.2)
        rest = [l for l in lines if l.split("\t", 1)[1] not in listed]
        if current is not None:
            failed.append({"file": current, "exit": describe_exit(code), "log": log_tail(log)})
            rest = [l for l in rest if l.split("\t", 1)[1] != current]
        elif rest and rest == lines:
            raise SystemExit("the worker failed before listing anything:\n" + log_tail(log, 40))
        lines = rest
    plugins = []
    for r in found:
        if r["format"] == "vst" and (not r.get("editor") or r.get("shell")):
            continue
        plugins.append(r)
    data = {"plugins": plugins, "list_failed": failed}
    cache.write_text(json.dumps(data, indent=1))
    return data


def run_jobs(args, display, plugins, mode, save=None):
    """mode: "" or "gestures" (CLAP only)"""
    extra = {}
    if mode == "gestures":
        extra["SCORE_GUI_SWEEP_CLAP_GESTURES"] = "1"
    if args.steps:
        extra["SCORE_GUI_SWEEP_STEPS"] = args.steps
    queue = list(plugins)
    results = {}
    total = len(plugins)
    started = time.monotonic()
    by_uri = {f"{p['format']}|{p['key']}": p for p in plugins}

    def finish(rec):
        meta = by_uri.get(rec["uri"], {})
        rec = {**meta, **{k: v for k, v in rec.items() if v not in ("", None) or k not in meta}, "mode": mode}
        results[rec["uri"]] = rec

    while queue:
        if not display.alive():
            raise SystemExit("the private Xvfb died")
        wait_for_memory(args)
        batch = queue[: args.batch]
        del queue[: args.batch]
        w = Worker(args, display, extra,
                   [f"{p['format']}\t{p['key']}\t{p.get('file', '')}" for p in batch])
        current, phase, done = None, None, set()
        while True:
            for r in w.poll():
                if r.get("type") == "phase":
                    if r["phase"] == "done":
                        current, phase = None, None
                    else:
                        current, phase = r["uri"], r["phase"]
                elif r.get("type") == "result":
                    done.add(r["uri"])
                    finish(r)
            code = w.proc.poll()
            hung = code is None and current is not None and time.monotonic() - w.last_progress > args.timeout
            if code is not None or hung:
                break
            time.sleep(0.2)
        if hung:
            w.kill()
            code = None
        for r in w.poll():
            if r.get("type") == "result":
                done.add(r["uri"])
                finish(r)
        w.close()
        rest = [p for p in batch if f"{p['format']}|{p['key']}" not in done]
        culprit = current
        if culprit is not None:
            prev = results.get(culprit, {})
            finish({
                **prev,
                "type": "result",
                "uri": culprit,
                "status": "hang" if hung else "crash",
                "phase": phase,
                "exit": describe_exit(code),
                "log": log_tail(w.log),
                "worker_log": str(w.log),
            })
            rest = [p for p in rest if f"{p['format']}|{p['key']}" != culprit]
        elif rest:
            if len(rest) == len(batch):
                raise SystemExit(f"the worker failed before probing anything ({describe_exit(code)}):\n"
                                 + log_tail(w.log, 40))
        queue[:0] = rest
        if save:
            save(results)
        if args.progress:
            sys.stderr.write(f"\r[{mode or 'as is'}] {len(results)}/{total} plug-ins, "
                             f"{time.monotonic() - started:.0f} s   ")
            sys.stderr.flush()
    if args.progress:
        sys.stderr.write("\n")
    return results


# ---------------------------------------------------------------------------
# Judging


def close_to(a, b, lo, hi, rel=0.02):
    if a is None or b is None:
        return None
    span = (hi - lo) if (hi is not None and lo is not None and hi > lo) else 1.0
    return abs(a - b) <= rel * span + 1e-6


def judge(r):
    """The checks a record passes and fails: {check: True/False/None}"""
    v = {}
    st = r.get("status")
    v["crash"] = st in ("crash", "hang")
    o = r.get("open") or {}
    if st in ("crash", "hang") and not o:
        return v
    if st in ("no-editor", "instantiate-failed", "not-found", "not-a-plugin", "unknown-format"):
        return v
    v["opened"] = bool(o.get("created")) and bool(o.get("x_viewable") or o.get("floating"))
    w, h = o.get("x_w") or 0, o.get("x_h") or 0
    v["sane_size"] = v["opened"] and MIN_SIDE <= w <= SCREEN[0] * 2 and MIN_SIDE <= h <= SCREEN[1] * 2
    if r.get("format") != "jsfx" and v["opened"] and not o.get("floating"):
        cw, ch = o.get("x_child_w") or 0, o.get("x_child_h") or 0
        v["embedded"] = (o.get("x_descendants") or 0) > 0
        v["child_fits"] = v["embedded"] and abs(cw - w) <= SIZE_SLACK and abs(ch - h) <= SIZE_SLACK
    v["renders"] = v["opened"] and not o.get("shot_blank", True)
    if v["opened"] and o.get("resizable") == 1 and "qt_fixed" in o:
        # A window manager does not resize a window whose sizes are fixed
        v["user_resizable"] = not o["qt_fixed"]
    rs = r.get("resize")
    if rs:
        ok = rs.get("alive") and not rs.get("shot_blank", True)
        if r.get("format") != "jsfx":
            ok = ok and abs((rs.get("x_child_w") or 0) - (rs.get("x_w") or 0)) <= SIZE_SLACK \
                and abs((rs.get("x_child_h") or 0) - (rs.get("x_h") or 0)) <= SIZE_SLACK
        pw = rs.get("plugin_w")
        if pw and pw > 0:
            ok = ok and abs(pw - (rs.get("x_w") or 0)) <= SIZE_SLACK
        v["resize"] = bool(ok)
    it = r.get("interact")
    if it:
        v["gui_to_score"] = bool(it.get("changed"))
        if it.get("changed"):
            # A control made for the parameter is an undo step of its own; the
            # value set by the gesture is the one asked for
            values = [c for c in it.get("commands", []) if "Create" not in c]
            v["one_undo_step"] = len(values) == 1
    hs = r.get("host")
    if hs and "to" in hs:
        pv = hs.get("plugin_after")
        v["host_to_plugin"] = close_to(pv, hs.get("to"), hs.get("lo"), hs.get("hi")) if pv is not None else None
        noise = max(hs.get("noise") or 0, 0)
        v["host_to_editor"] = (hs.get("diff") or 0) >= max(MIN_DIFF, 3 * noise)
    c = r.get("close")
    if c:
        v["closes"] = bool(c.get("ui_reset") and c.get("x_gone"))
        v["reopens"] = bool(c.get("reopened") and not c.get("shot_blank", True) and c.get("reclosed"))
    p = r.get("play")
    if p:
        v["play_open"] = bool(p.get("opened") and not p.get("shot_blank", True) and p.get("closed"))
        if it and it.get("changed") and "interact_changed" in p:
            v["play_gui_to_score"] = bool(p.get("interact_changed"))
    ps = r.get("persist")
    if ps and ps.get("changed"):
        kept = close_to(ps.get("score_after"), ps.get("score_before"), ps.get("lo"), ps.get("hi"), 1e-3)
        if ps.get("plugin_before") is not None and ps.get("plugin_after") is not None:
            kept = kept and close_to(ps.get("plugin_after"), ps.get("plugin_before"), ps.get("lo"), ps.get("hi"), 1e-3)
        v["persist"] = bool(kept)
        v["persist_reopens"] = bool(ps.get("reopened") and not ps.get("shot_blank", True))
    if st == "crash" or st == "hang":
        v["crash"] = True
    return v


CHECKS = [
    ("opened", "editor opened and mapped"),
    ("sane_size", "sane size"),
    ("embedded", "plug-in window embedded"),
    ("child_fits", "plug-in window fills the host window"),
    ("renders", "not blank"),
    ("user_resizable", "the window of a resizable editor can be resized"),
    ("resize", "resize followed"),
    ("gui_to_score", "editor change reaches score"),
    ("one_undo_step", "one undo step per gesture"),
    ("host_to_plugin", "score change reaches the plug-in"),
    ("host_to_editor", "score change shows in the editor"),
    ("closes", "closes"),
    ("reopens", "reopens"),
    ("play_open", "opens while playing"),
    ("play_gui_to_score", "editor change reaches score while playing"),
    ("persist", "editor change kept through save/load"),
    ("persist_reopens", "reopens after load"),
]


def fmt_gestures(g):
    if not g or not isinstance(g, dict):
        return ""
    if "begin" in g:
        return f"begin {g['begin']} / perform {g['perform']} / end {g['end']}"
    return (f"flush {g['flushes']}, out main b/v/e {g['main_begin']}/{g['main_values']}/{g['main_end']}, "
            f"audio b/v/e {g['audio_begin']}/{g['audio_values']}/{g['audio_end']}")


def write_report(args, data, results, compare):
    out = []
    plugins = data["plugins"]
    out.append("# Plug-in editor sweep\n")
    out.append(f"{len(plugins)} plug-ins listed; screenshots in `{Path(args.out) / 'shots'}`.\n")
    by_fmt = defaultdict(list)
    for r in results.values():
        if r.get("mode", "") == "":
            by_fmt[r["format"]].append(r)
    out.append("## Totals\n")
    header = ["format", "probed", "no editor", "load failed", "crash", "hang"] + [c for c, _ in CHECKS]
    out.append("Each check: passed / judged.\n")
    out.append("| " + " | ".join(header) + " |")
    out.append("|" + "---|" * len(header))
    for fmt in FORMATS:
        rs = by_fmt.get(fmt, [])
        if not rs:
            continue
        st = Counter(r.get("status") for r in rs)
        row = [fmt, str(len(rs)), str(st["no-editor"]),
               str(st["instantiate-failed"] + st["not-found"] + st["not-a-plugin"]),
               str(st["crash"]), str(st["hang"])]
        for c, _ in CHECKS:
            judged = [r["verdict"].get(c) for r in rs if r["verdict"].get(c) is not None]
            row.append(f"{sum(1 for x in judged if x)}/{len(judged)}")
        out.append("| " + " | ".join(row) + " |")
    out.append("\n" + "\n".join(f"- `{c}`: {d}" for c, d in CHECKS) + "\n")

    crashes = [r for r in results.values() if r.get("status") in ("crash", "hang")]
    if crashes:
        out.append(f"## Crashes and hangs ({len(crashes)})\n")
        for r in sorted(crashes, key=lambda r: (r.get("format"), r.get("name", ""))):
            out.append(f"### {r.get('name') or r['key']} ({r['format']}{', gestures sent' if r.get('mode') else ''})\n")
            out.append(f"`{r['uri']}`: {r['status']} in step **{r.get('phase')}** ({r.get('exit')}), "
                       f"log `{r.get('worker_log')}`\n")
            if r.get("log"):
                out.append("```\n" + r["log"][-4000:] + "\n```\n")

    if data.get("list_failed"):
        out.append(f"## Files that crashed the listing ({len(data['list_failed'])})\n")
        for f in data["list_failed"]:
            out.append(f"- `{f['file']}`: {f['exit']}")
        out.append("")

    out.append("## Failed checks per plug-in\n")
    for fmt in FORMATS:
        rs = sorted(by_fmt.get(fmt, []), key=lambda r: r.get("name") or r["key"])
        lines = []
        for r in rs:
            failed = [c for c, _ in CHECKS if r["verdict"].get(c) is False]
            if not failed:
                continue
            o = r.get("open") or {}
            extra = f" window {o.get('x_w')}x{o.get('x_h')}, child {o.get('x_child_w')}x{o.get('x_child_h')}"
            if o.get("plugin_w"):
                extra += f", plug-in says {o.get('plugin_w')}x{o.get('plugin_h')}"
            it = r.get("interact") or {}
            if it.get("changed"):
                extra += f"; interact: {it.get('kind')} changed {', '.join(it.get('controls', []))}, undo steps {it.get('undo_steps')}"
                g = it.get("gestures")
                if g and g != "null":
                    extra += f", gestures {fmt_gestures(g)}"
            lines.append(f"- **{r.get('name') or r['key']}** `{r['key']}`: {', '.join(failed)};{extra}")
        if lines:
            out.append(f"### {fmt} ({len(lines)})\n")
            out += lines
            out.append("")

    if compare:
        out.append("## CLAP: gestures sent around score's changes (a04edef446) vs not (as built)\n")
        out.append("Each row is a CLAP plug-in with an editor; *as is* is score's host as built, "
                   "*gestures* adds PARAM_GESTURE_BEGIN before score's first value and "
                   "PARAM_GESTURE_END through flush() 200 ms after its last, as a04edef446 did.\n")
        out.append("| plug-in | differs | as is | gestures |")
        out.append("|---|---|---|---|")
        keys = ["crash", "gui_to_score", "host_to_plugin", "host_to_editor", "play_gui_to_score", "persist"]
        rows = []
        for uri, (a, b) in sorted(compare.items(), key=lambda kv: (kv[1][0] or kv[1][1]).get("name", "")):
            def summ(r):
                if not r:
                    return "not run"
                if r.get("status") in ("crash", "hang"):
                    return f"**{r['status']}** in {r.get('phase')}"
                if r.get("status") != "ok":
                    return r.get("status")
                v = r["verdict"]
                parts = []
                for k in keys[1:]:
                    if v.get(k) is not None:
                        parts.append(f"{k} {'yes' if v[k] else 'NO'}")
                it = r.get("interact") or {}
                if it.get("gestures") and it.get("gestures") != "null":
                    g = it["gestures"]
                    parts.append(f"editor gestures {g.get('main_begin', 0) + g.get('audio_begin', 0)}/"
                                 f"{g.get('main_end', 0) + g.get('audio_end', 0)}")
                hs = r.get("host") or {}
                if hs.get("gestures") and isinstance(hs["gestures"], dict):
                    parts.append(f"sent b/e {hs['gestures'].get('injected_begin', 0)}/{hs['gestures'].get('injected_end', 0)}")
                return ", ".join(parts)
            diffs = []
            ra, rb = a or {}, b or {}
            for k in keys:
                va = ra.get("verdict", {}).get(k) if k != "crash" else ra.get("status") in ("crash", "hang")
                vb = rb.get("verdict", {}).get(k) if k != "crash" else rb.get("status") in ("crash", "hang")
                if va != vb:
                    diffs.append(k)
            name = ra.get("name") or rb.get("name") or uri
            rows.append((bool(diffs), f"| {name} | {', '.join(diffs) or '-'} | {summ(a)} | {summ(b)} |"))
        rows.sort(key=lambda x: not x[0])
        out += [r for _, r in rows]
        out.append("")
    report = Path(args.out) / "report.md"
    report.write_text("\n".join(out) + "\n")
    return report


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--worker", required=True)
    ap.add_argument("--build", required=True)
    ap.add_argument("--out", default="plugin-gui-sweep", help="where results go")
    ap.add_argument("--formats", default=",".join(FORMATS))
    ap.add_argument("--filter", help="regular expression on format|key|name|file")
    ap.add_argument("--batch", type=int, default=8)
    ap.add_argument("--limit", type=int, default=0, help="at most this many plug-ins per format")
    ap.add_argument("--exclude", help="regular expression on format|key|name|file: those are left out...")
    ap.add_argument("--exclude-keep", type=int, default=0,
                    help="...but for the first N of each format (a large family of alike plug-ins)")
    ap.add_argument("--timeout", type=float, default=90, help="seconds without progress before a hang")
    ap.add_argument("--steps", default="", help="only these steps (comma-separated)")
    ap.add_argument("--no-compare", action="store_true", help="CLAP only as is, not also with gestures")
    ap.add_argument("--display", type=int, help="Xvfb display number")
    ap.add_argument("--mem-max", default="6G")
    ap.add_argument("--min-free-gb", type=float, default=12)
    ap.add_argument("--no-scope", action="store_true")
    ap.add_argument("--relist", action="store_true")
    ap.add_argument("--rejudge", action="store_true", help="judge results.json again, run nothing")
    ap.add_argument("--resume", action="store_true",
                    help="keep the results.json there and probe only what it lacks")
    ap.add_argument("--progress", action="store_true")
    ap.add_argument("--require-opt-in", action="store_true", help="skip (77) unless SCORE_PLUGIN_GUI_SWEEP=1")
    args = ap.parse_args()
    if args.require_opt_in and os.environ.get("SCORE_PLUGIN_GUI_SWEEP") != "1":
        print("skipped: set SCORE_PLUGIN_GUI_SWEEP=1 to run the plug-in editor sweep")
        return 77
    args.formats = [f for f in args.formats.split(",") if f]
    args.worker = os.path.abspath(args.worker)
    args.build = os.path.abspath(args.build)
    args.out = os.path.abspath(os.path.expanduser(args.out))
    for tool in ("Xvfb", "xdotool"):
        if not shutil.which(tool):
            print(f"skipped: {tool} not found")
            return 77
    Path(args.out).mkdir(parents=True, exist_ok=True)
    res_path = Path(args.out) / "results.json"

    if args.rejudge:
        saved = json.loads(res_path.read_text())
        data = {"plugins": saved["plugins"], "list_failed": saved.get("list_failed", [])}
        results = {r["uri"] + ("#" + r["mode"] if r.get("mode") else ""): r for r in saved["results"]}
    else:
        display = Display(args.display)
        try:
            data = list_plugins(args, display)
            plugins = sorted((p for p in data["plugins"] if p["format"] in args.formats),
                             key=lambda p: args.formats.index(p["format"]))
            if args.filter:
                rx = re.compile(args.filter)
                plugins = [p for p in plugins if rx.search(describe(p))]
            if args.exclude:
                rx = re.compile(args.exclude)
                kept, n = [], Counter()
                for p in plugins:
                    if rx.search(describe(p)):
                        if n[p["format"]] >= args.exclude_keep:
                            continue
                        n[p["format"]] += 1
                    kept.append(p)
                plugins = kept
            if args.limit:
                kept, n = [], Counter()
                for p in plugins:
                    if n[p["format"]] < args.limit:
                        kept.append(p)
                        n[p["format"]] += 1
                plugins = kept
            results = {}
            if args.resume and res_path.exists():
                for r in json.loads(res_path.read_text())["results"]:
                    results[r["uri"] + ("#" + r["mode"] if r.get("mode") else "")] = r
            # Format after format, the report written after each
            for fmt in args.formats:
                batch = [p for p in plugins if p["format"] == fmt]
                if not batch:
                    continue
                todo = [p for p in batch if f"{p['format']}|{p['key']}" not in results]

                def save(found, suffix=""):
                    # What a batch found is kept at once: a stopped sweep resumes from there
                    merged = {**results, **{k + suffix: v for k, v in found.items()}}
                    res_path.write_text(json.dumps(
                        {"plugins": data["plugins"], "list_failed": data.get("list_failed", []),
                         "results": list(merged.values())}, indent=1))

                for uri, r in run_jobs(args, display, todo, "", save).items():
                    results[uri] = r
                if fmt == "clap" and not args.no_compare:
                    clap = [p for p in batch
                            if results.get(f"clap|{p['key']}", {}).get("status")
                            not in ("no-editor", "instantiate-failed")
                            and f"clap|{p['key']}#gestures" not in results]
                    for uri, r in run_jobs(args, display, clap, "gestures",
                                           lambda f: save(f, "#gestures")).items():
                        results[uri + "#gestures"] = r
                finish_report(args, data, results, res_path)
        finally:
            display.close()
    report = finish_report(args, data, results, res_path)
    print(f"report: {report}")
    return 0


def finish_report(args, data, results, res_path):
    for r in results.values():
        r["verdict"] = judge(r)
    compare = {}
    for key, r in results.items():
        if r.get("format") != "clap":
            continue
        uri = r["uri"]
        a, b = compare.get(uri, (None, None))
        if r.get("mode") == "gestures":
            b = r
        else:
            a = r
        compare[uri] = (a, b)
    compare = {k: v for k, v in compare.items() if v[1] is not None
               and (v[0] or {}).get("status") not in ("no-editor",)}
    res_path.write_text(json.dumps({"plugins": data["plugins"], "list_failed": data.get("list_failed", []),
                                    "results": list(results.values())}, indent=1))
    return write_report(args, data, results, compare)


if __name__ == "__main__":
    sys.exit(main())
