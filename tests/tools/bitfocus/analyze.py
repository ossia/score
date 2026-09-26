#!/usr/bin/env python3
# Compares, per module, what the reference host and score's host observed.
import json, sys, re
from collections import Counter, defaultdict
from pathlib import Path


def jl(p):
    out = []
    try:
        for line in open(p, errors="replace"):
            line = line.strip()
            if not line:
                continue
            try:
                out.append(json.loads(line))
            except json.JSONDecodeError:
                pass
    except FileNotFoundError:
        pass
    return out


def trace(p):
    """[bitfocus] -> / <- lines of the score driver, split per handler (widget, device)."""
    msgs = []
    try:
        for line in open(p, errors="replace"):
            m = re.match(r"\[bitfocus\] (->|<-) (.*)$", line.rstrip("\n"))
            if not m:
                continue
            try:
                o = json.loads(m.group(2))
            except json.JSONDecodeError:
                continue
            o["_dir"] = m.group(1)
            try:
                o["_payload"] = json.loads(o["payload"]) if isinstance(o.get("payload"), str) and o["payload"] else None
            except json.JSONDecodeError:
                o["_payload"] = None
            if o["_payload"] is not None and not isinstance(o["_payload"], dict):
                o["_payload"] = {"message": str(o["_payload"])}
            msgs.append(o)
    except FileNotFoundError:
        pass
    regs = [i for i, o in enumerate(msgs) if o.get("name") == "register"]
    if len(regs) >= 2:
        return msgs[: regs[1]], msgs[regs[1]:]
    return msgs, []


def windows(events, begin_ev="action"):
    acts = [e for e in events if e.get("ev") == "action"]
    end = next((e["t"] for e in events if e.get("ev") == "actions-end"), None)
    res = []
    for i, a in enumerate(acts):
        t1 = acts[i + 1]["t"] if i + 1 < len(acts) else (end or a["t"] + 1) + 0.3
        res.append((a["id"], a["t"], t1))
    return res


def catch_payloads(catch, t0, t1, background):
    out = []
    for e in catch:
        if e.get("ev") == "data" and t0 <= e["t"] < t1:
            key = (e["p"], e["port"], e["d"])
            if key in background:
                continue
            out.append(key)
    return Counter(out)


def background_of(catch, t_begin, wins=()):
    bg = {(e["p"], e["port"], e["d"]) for e in catch if e.get("ev") == "data" and e["t"] < t_begin}
    # Payloads recurring in several action windows are polling, not the actions
    seen = Counter()
    for _, t0, t1 in wins:
        for k in {(e["p"], e["port"], e["d"]) for e in catch if e.get("ev") == "data" and t0 <= e["t"] < t1}:
            seen[k] += 1
    return bg | {k for k, n in seen.items() if n >= 3}


def analyze(d: Path, prefix="score"):
    r = {"module": d.name}
    ref = jl(d / "ref.jsonl")
    sc = jl(d / f"{prefix}.jsonl")
    cfg = {}
    try:
        cfg = json.load(open(d / "ref.jsonl.config.json"))
    except Exception:
        pass
    rcatch = jl(d / "ref.catch")
    scatch = jl(d / f"{prefix}.catch")
    widget_tr, dev_tr = trace(d / f"{prefix}.stderr")

    # --- lifecycle
    r["ref_init"] = any(e.get("ev") == "init-ok" and e.get("phase") == "run" for e in ref)
    r["ref_init_err"] = next((e.get("message") for e in ref if e.get("ev") == "init-failed" and e.get("phase") == "run"), None)
    init_calls = {o["callbackId"] for o in dev_tr if o.get("name") == "init" and o["_dir"] == "->"}
    init_resp = [o for o in dev_tr if o.get("direction") == "response" and o["_dir"] == "<-" and o.get("callbackId") in init_calls]
    r["score_init"] = bool(init_resp) and bool(init_resp[0].get("success"))
    r["score_init_err"] = (init_resp[0]["_payload"] or {}).get("message") if init_resp and not init_resp[0].get("success") else None
    reg = next((e for e in sc if e.get("ev") == "registered"), None)
    r["score_registered"] = bool(reg and reg.get("ok"))
    try:
        r["score_rc"] = int((d / f"{prefix}.rc").read_text())
    except Exception:
        r["score_rc"] = None

    # --- definitions
    rsum = next((e for e in ref if e.get("ev") == "summary"), {})
    ssum = next((e for e in sc if e.get("ev") == "summary"), {})
    r["ref_defs"] = (rsum.get("actions"), rsum.get("feedbacks"), rsum.get("variables"))
    tree = ssum.get("tree") or {}
    kids = {c["name"]: c for c in tree.get("children", [])}
    def count(name, with_param=False):
        n = kids.get(name, {}).get("children", [])
        return sum(1 for c in n if (not with_param or "type" in c))
    r["score_tree"] = (count("action"), count("feedback"), count("variable"))
    r["score_model"] = (ssum.get("actions"), ssum.get("feedbacks"), ssum.get("variables"))
    # Option nodes with no parameter
    noparam = []
    for grp in ("action", "feedback"):
        for n in kids.get(grp, {}).get("children", []):
            for o in n.get("children", []):
                if "type" not in o:
                    noparam.append(f"{grp}/{n['name']}/{o['name']}")
    r["options_without_param"] = noparam

    # --- configuration
    wc = next((e.get("config") for e in sc if e.get("ev") == "widget-config"), None)
    rc = cfg.get("config", {})
    probe_init = next((e for e in ref if e.get("ev") == "init-ok" and e.get("phase") == "probe"), None)
    if probe_init and isinstance(probe_init.get("updatedConfig"), dict):
        rc = dict(probe_init["updatedConfig"], **cfg.get("overrides", {}))
    # score keeps secrets along with the rest of the configuration
    rc = dict(cfg.get("secrets") or {}, **rc)
    diffs = {}
    if wc is not None:
        for k in set(rc) | set(wc):
            a, b = rc.get(k, "<missing>"), wc.get(k, "<missing>")
            if a != b or type(a) != type(b):
                diffs[k] = (a, b)
    r["config_diffs"] = diffs
    # what init/updateConfigAndLabel actually carried
    sent_cfg = None
    for o in dev_tr:
        if o.get("name") in ("updateConfigAndLabel",) and o["_dir"] == "->":
            sent_cfg = (o["_payload"] or {}).get("config")
    r["score_sent_config"] = sent_cfg

    # --- actions
    rres = {e["id"]: e for e in ref if e.get("ev") == "action-result"}
    ropts = {e["id"]: e.get("options") for e in ref if e.get("ev") == "action"}
    exec_calls = {o["callbackId"]: o for o in dev_tr if o.get("name") == "executeAction" and o["_dir"] == "->"}
    sres = {}
    sopts = {}
    for o in dev_tr:
        if o.get("direction") == "response" and o["_dir"] == "<-" and o.get("callbackId") in exec_calls:
            call = exec_calls[o["callbackId"]]
            aid = call["_payload"]["action"]["actionId"]
            p = o["_payload"]
            ok = bool(o.get("success")) and (p is None or p.get("success", True))
            sres[aid] = {"success": ok, "error": (p or {}).get("errorMessage") or (p or {}).get("message")}
    for c in exec_calls.values():
        a = c["_payload"]["action"]
        sopts[a["actionId"]] = a.get("options")
    r["actions_ref_ok"] = sum(1 for v in rres.values() if v.get("success"))
    r["actions_ref_total"] = len(rres)
    r["actions_score_ok"] = sum(1 for v in sres.values() if v.get("success"))
    r["actions_score_sent"] = len(exec_calls)
    r["actions_score_answered"] = len(sres)
    fail_diff = []
    for aid, v in rres.items():
        s = sres.get(aid)
        if v.get("success") and s is not None and not s["success"]:
            fail_diff.append((aid, s["error"], ropts.get(aid), sopts.get(aid)))
        elif v.get("success") and s is None:
            fail_diff.append((aid, "not executed by score", ropts.get(aid), sopts.get(aid)))
    r["actions_failing_only_in_score"] = fail_diff
    # option mismatches
    optdiff = []
    for aid, ro in ropts.items():
        so = sopts.get(aid)
        if so is None:
            continue
        for k in set(ro or {}) | set(so or {}):
            a = (ro or {}).get(k, "<missing>")
            b = (so or {}).get(k, "<missing>")
            if a != b or type(a) != type(b):
                optdiff.append((aid, k, a, b))
    r["action_option_diffs"] = optdiff

    # --- bytes on the wire per action
    rw = windows(ref)
    sw = windows(sc)
    rbegin = next((e["t"] for e in ref if e.get("ev") == "actions-begin"), 1e18)
    sbegin = next((e["t"] for e in sc if e.get("ev") == "actions-begin"), 1e18)
    bg = background_of(rcatch, rbegin, rw) | background_of(scatch, sbegin, sw)
    rwire = {aid: catch_payloads(rcatch, t0, t1, bg) for aid, t0, t1 in rw}
    swire = {aid: catch_payloads(scatch, t0, t1, bg) for aid, t0, t1 in sw}
    wire_diff = []
    order = [aid for aid, _, _ in rw]
    def near(wire, i):
        keys = set()
        for j in (i - 1, i, i + 1):
            if 0 <= j < len(order) and order[j] in wire:
                keys |= set(wire[order[j]])
        return keys
    for i, aid in enumerate(order):
        rp = rwire[aid]
        sp = swire.get(aid)
        if sp is None:
            if rp:
                wire_diff.append((aid, "score did not run it", len(rp)))
            continue
        only_r = set(rp) - near(swire, i)
        only_s = set(sp) - near(rwire, i)
        if only_r or only_s:
            wire_diff.append((aid, [(p, port, bytes.fromhex(x)[:80]) for p, port, x in list(only_r)[:2]],
                              [(p, port, bytes.fromhex(x)[:80]) for p, port, x in list(only_s)[:2]]))
    r["wire_actions_with_output_ref"] = sum(1 for v in rwire.values() if v)
    # Actions whose output differs between two runs of the reference are noise
    ref2 = jl(d / "ref2.jsonl")
    noisy = set()
    if ref2:
        r2catch = jl(d / "ref2.catch")
        r2begin = next((e["t"] for e in ref2 if e.get("ev") == "actions-begin"), 1e18)
        bg2 = bg | background_of(r2catch, r2begin, windows(ref2))
        r2wire = {aid: catch_payloads(r2catch, t0, t1, bg2) for aid, t0, t1 in windows(ref2)}
        rwire2 = {aid: catch_payloads(rcatch, t0, t1, bg2) for aid, t0, t1 in rw}
        for i, aid in enumerate(order):
            if set(rwire2[aid]) - near(r2wire, i) or set(r2wire.get(aid, {})) - near(rwire2, i):
                noisy.add(aid)
        r["ref2"] = True
    r["wire_noisy_actions"] = len(noisy)
    r["wire_diffs"] = [w for w in wire_diff if w[0] not in noisy]

    # --- variables / feedbacks
    rvals = rsum.get("variableValues") or {}
    svals = ssum.get("variableValues") or {}
    r["vars_ref_nonempty"] = sum(1 for v in rvals.values() if v not in ("", None))
    r["vars_score_with_param"] = count("variable", True)
    r["vars_score_values"] = len(svals)
    mism = []
    for k, v in rvals.items():
        if v in ("", None):
            continue
        if k not in svals:
            mism.append((k, v, "<no param>"))
        elif str(svals[k]) != str(v):
            mism.append((k, v, svals[k]))
    r["var_value_diffs"] = mism
    r["fb_ref_values"] = sum(len(e.get("values", [])) for e in ref if e.get("ev") == "fbvals")
    r["fb_score_values"] = sum(len((o["_payload"] or {}).get("values", [])) for o in dev_tr if o.get("name") == "updateFeedbackValues")
    r["fb_score_signals"] = ssum.get("fbSignals")

    # --- module errors
    def errs_ref():
        return [e.get("message") for e in ref if e.get("ev") == "log" and e.get("phase") == "run" and e.get("level") == "error"]
    def errs_score():
        out = []
        for o in dev_tr:
            if o.get("name") == "log-message" and o["_dir"] == "<-":
                p = o["_payload"] or {}
                if p.get("level") == "error":
                    out.append(p.get("message"))
        return out
    r["log_errors_ref"] = errs_ref()
    r["log_errors_score"] = errs_score()
    unhandled = Counter()
    for o in dev_tr:
        if o.get("direction") == "response" and o["_dir"] == "<-" and not o.get("success"):
            unhandled[str((o["_payload"] or {}).get("message"))[:120]] += 1
    r["score_failed_responses"] = dict(unhandled)
    host_unhandled = Counter()
    try:
        for line in open(d / f"{prefix}.stderr", errors="replace"):
            m = re.match(r'Unhandled:\s+(.*)', line.strip())
            if m:
                host_unhandled[m.group(1)] += 1
    except FileNotFoundError:
        pass
    r["host_unhandled_calls"] = dict(host_unhandled)
    return r


if __name__ == "__main__":
    root = Path(sys.argv[1])
    prefix = sys.argv[2] if len(sys.argv) > 2 else "score"
    res = [analyze(d, prefix) for d in sorted(root.iterdir()) if d.is_dir()]
    json.dump(res, open(root / f"analysis-{prefix}.json", "w"), indent=1, default=str)
    print(f"{len(res)} modules")
