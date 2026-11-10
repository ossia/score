"""The phases of tests/fixtures/score_test/LivePlugin.hpp, for the sweep drivers.

With --live, a driver has its worker add each plug-in to a playing document in
every way a user can (SCORE_SWEEP_LIVE=1), instead of its usual probe. The
worker records a "phase" line before each phase, as it does for its own, and a
"result" with "mode": "live" and "live": {phase: {"verdict", "detail"}}.

Verdicts on a plug-in:
  host  a phase failed, or the worker crashed or hung in one: score did not
        keep the plug-in running through what the phase does. Confirm alone
        (--batch 1, twice) and against the usual sweep before blaming score.
"""

import os
from collections import Counter, defaultdict

PHASES = [
    "live-drop-interval",
    "live-drop-stopped-interval",
    "live-drop-nodal",
    "live-drop-box-past",
    "live-drop-box-playhead",
    "live-drop-box-future",
    "live-start-offset",
    "live-loop",
    "live-speed",
    "live-paused",
    "live-remove-undo-redo",
    "live-undo-redo-add",
    "live-copy-paste",
    "live-duplicate",
    "live-preset-user",
    "live-preset-builtin",
    "live-preset-change",
    "live-drop-on-cable",
    "live-replace",
    "live-multi-drop",
    "live-editor",
    "live-audio-settings",
]


def add_arguments(ap):
    ap.add_argument(
        "--live",
        action="store_true",
        help="add each plug-in to a playing document in every way instead of the usual probe",
    )
    ap.add_argument("--live-phases", help="only these live phases, comma-separated")


def worker_env(args):
    """What the worker is told to do, to add to its environment"""
    if not getattr(args, "live", False):
        return {}
    env = {"SCORE_SWEEP_LIVE": "1"}
    if getattr(args, "live_phases", None):
        env["SCORE_SWEEP_LIVE_PHASES"] = args.live_phases
    return env


def is_live(r):
    return r.get("mode") == "live" or str(r.get("phase") or "").startswith("live-")


def judge(r):
    """The verdicts on one live record: (severity, code, message)"""
    v = []
    add = lambda sev, code, msg: v.append({"severity": sev, "code": code, "message": msg})
    status = r.get("status")
    if status in ("crash", "hang"):
        where = r.get("phase") or "?"
        what = "crashed" if status == "crash" else "no progress for the timeout"
        add("host", f"live-{status}", f"{what} during {where} ({r.get('exit', '')})")
        return v
    if status not in (None, "ran"):
        add("warn", status, f"not added to a document: {status}")
        return v
    for phase, res in (r.get("live") or {}).items():
        if res.get("verdict") == "fail":
            add("host", "live-fail", f"{phase}: {res.get('detail', '')}")
    return v


def outcome(r, phase):
    """What happened to a record in a phase: ok, skipped, fail, crash, hang or
    not-reached"""
    live = r.get("live") or {}
    if phase in live:
        return live[phase].get("verdict", "?")
    if r.get("status") in ("crash", "hang"):
        return r["status"] if r.get("phase") == phase else "not-reached"
    return "not-reached"


def report_lines(records, name=lambda r: r.get("name") or r.get("uri") or "?"):
    """The live section of a report.md, for the records that are live"""
    live = [r for r in records if is_live(r)]
    if not live:
        return []
    lines = ["## Added while playing", ""]
    lines.append(f"{len(live)} plug-ins through the phases of score_test/LivePlugin.hpp.")
    lines.append("")
    lines.append("| phase | ok | skipped | fail | crash | hang |")
    lines.append("|---|---|---|---|---|---|")
    failures = defaultdict(list)
    for phase in PHASES:
        c = Counter(outcome(r, phase) for r in live)
        if not any(c[k] for k in ("ok", "skipped", "fail", "crash", "hang")):
            continue
        lines.append(
            f"| {phase} | {c['ok']} | {c['skipped']} | {c['fail']} | {c['crash']} | {c['hang']} |"
        )
        for r in live:
            o = outcome(r, phase)
            if o in ("fail", "crash", "hang"):
                detail = (r.get("live") or {}).get(phase, {}).get("detail") or r.get("exit") or ""
                failures[phase].append(f"- **{name(r)}**: {o} {detail}".rstrip())
    lines.append("")
    for phase in PHASES:
        if failures[phase]:
            lines.append(f"### {phase}")
            lines.append("")
            lines.extend(failures[phase])
            lines.append("")
    return lines
