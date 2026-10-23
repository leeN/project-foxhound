#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Differential taint test: runs driver.js under an interpreter-only shell and
under several JIT configurations, and diffs every configuration against the
interpreter, case by case.

Exits with status 1 when a hard mismatch is found. Mismatches that disappear
once "function" operations or operation arguments are ignored are reported
separately and do not fail the run.
"""

import argparse
import collections
import concurrent.futures
import json
import os
import shlex
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

REFERENCE = "interp"
CONFIGS = {
    "interp": "--no-blinterp --no-baseline --no-ion",
    "blinterp": "--blinterp-eager --no-baseline --no-ion",
    "baseline": "--baseline-eager --no-ion",
    "ion-eager": "--ion-eager --ion-offthread-compile=off",
    "default": "",
    "fast-warmup": "--fast-warmup",
}


def run_config(js, name, flags, args):
    def shard(i):
        cmd = (
            [js]
            + shlex.split(flags)
            + shlex.split(args.extra)
            + [os.path.join(HERE, "driver.js"), HERE, str(i), str(args.shards)]
            + [args.filter or "", str(args.scale)]
        )
        try:
            p = subprocess.run(
                cmd, capture_output=True, text=True, timeout=args.timeout
            )
            return p.stdout, p.stderr, p.returncode
        except subprocess.TimeoutExpired as e:
            return (e.stdout or b"").decode(), "timeout", -1

    results = {}
    errors = []
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for i, (out, err, rc) in enumerate(pool.map(shard, range(args.shards))):
            if rc != 0:
                errors.append(f"shard {i}: exit {rc}: {err.strip()[-500:]}")
            for line in out.splitlines():
                if line.startswith("{"):
                    o = json.loads(line)
                    results[o["id"]] = o
    if args.out:
        with open(os.path.join(args.out, name + ".jsonl"), "w") as f:
            for id in sorted(results):
                f.write(json.dumps(results[id]) + "\n")
    return results, errors


def merge_ranges(ranges):
    merged = []
    for r in ranges:
        if merged and merged[-1]["e"] == r["b"] and merged[-1]["f"] == r["f"]:
            merged[-1] = dict(merged[-1], e=r["e"])
        else:
            merged.append(r)
    return merged


def strip(v, drop_function=False, drop_args=False, drop_location=False):
    """Canonical form of a snapshot: adjacent ranges with the same flow are
    merged, and the selected parts of each operation are left out."""
    if isinstance(v, dict):
        r = {}
        for k, x in v.items():
            if k == "args" and drop_args:
                continue
            if k in ("fn", "line") and drop_location:
                continue
            if k == "f" and drop_function:
                x = [op for op in x if op["op"] != "function"]
            r[k] = strip(x, drop_function, drop_args, drop_location)
        if "t" in r and "s" in r:
            r["t"] = merge_ranges(r["t"])
        return r
    if isinstance(v, list):
        return [strip(x, drop_function, drop_args, drop_location) for x in v]
    return v


def strings(v):
    """All taint-carrying string snapshots in v, in order."""
    if isinstance(v, dict):
        if "t" in v and "s" in v:
            yield v
            return
        for x in v.values():
            yield from strings(x)
    elif isinstance(v, list):
        for x in v:
            yield from strings(x)


def first_difference(a, b):
    sa, sb = list(strings(a)), list(strings(b))
    if len(sa) != len(sb):
        return f"result shape differs: {len(sa)} vs {len(sb)} strings"
    for x, y in zip(sa, sb):
        if x["s"] != y["s"]:
            return f"value {x['s']!r} vs {y['s']!r}"
        rx = [(r["b"], r["e"]) for r in x["t"]]
        ry = [(r["b"], r["e"]) for r in y["t"]]
        if rx != ry:
            return f"ranges on {x['s'][:40]!r}: {rx} vs {ry}"
        for r1, r2 in zip(x["t"], y["t"]):
            for i in range(max(len(r1["f"]), len(r2["f"]))):
                o1 = r1["f"][i] if i < len(r1["f"]) else None
                o2 = r2["f"][i] if i < len(r2["f"]) else None
                if o1 != o2:
                    return f"range [{r1['b']},{r1['e']}) op #{i}: {fmt_op(o1)} vs {fmt_op(o2)}"
    return "non-string value differs: " + json.dumps(a)[:100] + " vs " + json.dumps(b)[:100]


def fmt_op(op):
    if op is None:
        return "<none>"
    return f"{op['op']}({'|'.join(op['args'])[:60]}) @{op['fn']}:{op['line']}"


def has_taint(v):
    return any(s["t"] for s in strings(v))


def classify(a, b):
    if a == b:
        return None
    if strip(a) == strip(b):
        return "soft: range fragmentation"
    if strip(a, drop_function=True) == strip(b, drop_function=True):
        return "soft: function op only"
    if strip(a, drop_args=True) == strip(b, drop_args=True):
        return "soft: arguments only"
    if strip(a, drop_location=True) == strip(b, drop_location=True):
        return "location differs"
    if has_taint(a) and not has_taint(b):
        return "taint lost"
    if has_taint(b) and not has_taint(a):
        return "taint gained"
    return "taint differs"


def snapshots(o):
    snaps = dict(o.get("out", {}))
    if "input" in o:
        snaps["input"] = o["input"]
    return snaps


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--js", default=os.environ.get("JS"), required="JS" not in os.environ,
                   help="path to the JS shell (or set $JS)")
    p.add_argument("--configs", default=",".join(c for c in CONFIGS if c != REFERENCE),
                   help="comma-separated configurations to compare against interp")
    p.add_argument("--extra", default="", help="extra shell flags for every configuration")
    p.add_argument("--filter", help="regexp over case ids (case/input/pattern)")
    p.add_argument("--scale", type=float, default=1, help="scale hot iteration counts")
    p.add_argument("--shards", type=int, default=os.cpu_count())
    p.add_argument("--jobs", type=int, default=os.cpu_count())
    p.add_argument("--timeout", type=int, default=900)
    p.add_argument("--out", help="directory to keep each configuration's output in")
    p.add_argument("--show", type=int, default=20, help="cases to list per category")
    args = p.parse_args()
    if args.out:
        os.makedirs(args.out, exist_ok=True)

    ref, errors = run_config(args.js, REFERENCE, CONFIGS[REFERENCE], args)
    print(f"{REFERENCE}: {len(ref)} cases")
    for e in errors:
        print(f"  ERROR {e}")
    hard = bool(errors)

    for id, o in sorted(ref.items()):
        out = o["out"]
        if "first" in out and "last" in out and out["first"] != out["last"] and "loop" not in id:
            print(f"  note: {id}: interp first != last: {first_difference(out['first'], out['last'])}")

    for name in args.configs.split(","):
        res, errors = run_config(args.js, name, CONFIGS[name], args)
        findings = collections.defaultdict(list)
        for id, o in sorted(ref.items()):
            x = res.get(id)
            if x is None:
                findings["missing"].append((id, ""))
                continue
            a, b = snapshots(o), snapshots(x)
            for key in sorted(set(a) | set(b)):
                kind = classify(a.get(key), b.get(key))
                if kind:
                    findings[kind].append((f"{id}:{key}", first_difference(a.get(key), b.get(key))))
        n_hard = sum(len(v) for k, v in findings.items() if not k.startswith("soft"))
        n_soft = sum(len(v) for k, v in findings.items() if k.startswith("soft"))
        print(f"\n== {name} ({CONFIGS[name] or 'no flags'}): {len(res)} cases, "
              f"{n_hard} hard, {n_soft} soft mismatches")
        for e in errors:
            print(f"  ERROR {e}")
        hard = hard or bool(errors) or n_hard > 0
        for kind in sorted(findings, key=lambda k: (k.startswith("soft"), k)):
            items = findings[kind]
            by_case = collections.defaultdict(list)
            for id, diff in items:
                by_case[id.split("/")[0]].append((id, diff))
            print(f"  -- {kind} ({len(items)} in {len(by_case)} cases)")
            for case, hits in list(by_case.items())[: args.show]:
                inputs = sorted({h[0].split("/")[1] for h in hits})
                patterns = sorted({h[0].split("/")[2].split(":")[0] for h in hits})
                print(f"     {case} [{','.join(inputs)}] [{','.join(patterns)}]")
                print(f"        {hits[0][0]}: {hits[0][1][:300]}")
            if len(by_case) > args.show:
                print(f"     ... {len(by_case) - args.show} more cases")

    return 1 if hard else 0


if __name__ == "__main__":
    sys.exit(main())
