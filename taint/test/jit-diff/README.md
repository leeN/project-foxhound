# Differential taint test across JIT tiers

Taint is correct when it does not depend on which tier ran the code. This
harness runs the same string operations in an interpreter-only shell and in
several JIT configurations, and diffs the full taint of every result: ranges,
operations, operation arguments and locations.

```
python3 taint/test/jit-diff/run.py --js obj-tf-release/dist/bin/js --out artifacts/jit-diff/run
```

- `cases.js` holds the cases. Each one is an expression over the input `s`.
- `driver.js` runs every case on six input kinds (Latin-1, two-byte, partially
  tainted, rope, long, tiny). It uses four access patterns, because the inline
  cache state is what differs between them:
  - `tainted`: tainted from the first call.
  - `switch`: warmed up on untainted input, then switched to tainted.
  - `alt`: tainted and untainted calls alternating.
  - `loop`: switched to tainted inside a hot loop, after an OSR compile.

  Each case prints one JSON line with the canonical taint of its results.
  Script hashes, file paths and columns are left out.
- `run.py` runs the driver under each configuration (`interp`, `blinterp`,
  `baseline`, `ion-eager`, `default`, `fast-warmup`), sharded over all cores,
  and compares each one against `interp`. Ion compiles on the main thread in
  every configuration, so a rerun gives the same results.

Mismatch categories:

| category | meaning | fails the run |
|---|---|---|
| `taint lost` / `taint gained` | one side has no taint at all | yes |
| `taint differs` | different ranges or operations | yes |
| `location differs` | same taint, but an operation reports a different function or line | yes |
| `soft: range fragmentation` | the same taint, split into ranges differently | no |
| `soft: function op only` | only the `function` operation differs. It is recorded by interpreter calls only | no |
| `soft: arguments only` | only operation arguments differ | no |

Useful options: `--filter REGEXP` over `case/input/pattern` ids, `--configs`,
`--extra` for flags added to every configuration (e.g. `--gc-zeal=...` on a
gczeal build), and `--scale` to shorten the hot loops.

## Counting taint guard hits

Every JIT fast path that reads or builds string contents sends tainted strings
to a slow path (`js/src/jit/TaintGuards.h`). With
`FOXHOUND_TAINT_GUARD_COUNTS=1` in the environment, each of those checks counts
how often it fires, and the counts go to stderr at exit:

```
[taint-guard] pid=2399255 Concat 10000
```

The setting is read when code is generated. Without it the checks are the same
single branch as before.
