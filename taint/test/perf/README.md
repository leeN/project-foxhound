# Browser benchmarks

Measures how much taint tracking costs, by running the same standard
benchmarks in Foxhound and in a stock Firefox built from the upstream commit
Foxhound is based on.

Everything is served from localhost out of benchmarks already vendored in the
tree, so no network access and no Talos machinery is involved:

| benchmark | source | unit |
| --- | --- | --- |
| SunSpider 1.0.1 | `third_party/webkit/PerformanceTests/SunSpider` | ms, lower is better |
| Kraken 1.1 | `testing/talos/talos/tests/kraken` | ms, lower is better |
| V8 benchmark v7 | `testing/talos/talos/tests/v8_7` | score, higher is better |
| Dromaeo DOM query | `testing/talos/talos/tests/dromaeo` | runs/s, higher is better |
| Dromaeo CSS selectors (jQuery) | `testing/talos/talos/tests/dromaeo` | runs/s, higher is better |
| Selector microbenchmark | `taint/test/perf/selectors` | ms, lower is better |

The first three are pure JavaScript. They contain no live call to a selector
API at all -- the only occurrences are inside string data, inside generated
source text and inside a comment -- so they cannot see a change to the DOM
instrumentation, and a flat result from them is not evidence that a DOM change
is free. The Dromaeo suites are the standard benchmarks for that: `dom-query`
is nine sub-tests of `getElementById`, `getElementsByTagName` and
`getElementsByName`, and the CSS selector suites run a few dozen selectors
through jQuery.

Two things to know before reading a selector result off Dromaeo alone. Its
fixture is full of ids, and describing an element stops walking as soon as it
finds one, so Dromaeo measures the cheap case. And a live list caches itself on
the node it was created for, so calling `getElementsByTagName` repeatedly on the
same root matches once and is free afterwards. The microbenchmark exists to
cover what that leaves out: a deep id-less fixture, two hundred matches in a
single `querySelectorAll`, and a fresh root per live-list lookup.

Each driver reports its results differently, so `benchmarks.py` gives each one
a small patch that hands the numbers back to the harness instead of writing
them into the page. Adding a benchmark means adding an entry there.

## Running it locally

```sh
python3 taint/test/perf/run_benchmarks.py --rounds 8 \
    --out results.json \
    vanilla=/path/to/firefox \
    foxhound=obj-tf-release/dist/bin/foxhound

python3 taint/test/perf/summarise.py results.json --baseline vanilla
```

`--benchmarks` takes a comma separated subset. Any number of builds can be
compared at once; `--baseline` picks which one the others are measured against.

Builds are run interleaved, one round at a time, with the order rotated between
rounds, so that drift over a long run is spread across the builds instead of
landing on whichever one went first.

## Reading the results

Every number comes with a 95% confidence interval, and a difference is only
marked significant when a Welch t-test says so. **A point estimate on its own
is not a result.** On an otherwise idle machine the spread between runs of the
same build has been measured at 20 to 30% for the shorter benchmarks, and CI
runners are shared with other tenants, so they are worse.

`--subtests` breaks each suite down by sub-test as well as reporting its total.
Prefer it whenever a change is expected to touch one operation rather than the
whole engine: a regression confined to a single sub-test disappears into a suite
total. Measured example, on the change that made selector lookups record the
matched element's XPath -- the DOM query total moved 1.7% and was not
significant, while `getElementById` inside it moved 31% and was. Reading only
the total would have passed that as no change.

Watch for a sub-test that is *not* instrumented, and use it as a control:
`getElementsByName` carries no taint operation, so in the same run it holds
still while the instrumented sub-tests move. Note also that a per-sub-test table
is a few dozen simultaneous comparisons, so a handful of them will read as
significant by chance -- an unexplained sub-test moving in the *improving*
direction is the usual sign that is what happened.

This is why the workflow runs weekly rather than per pull request, and why it
reports rather than fails. A benchmark gate tight enough to catch a real
regression would flap constantly on this hardware; one loose enough not to flap
would not catch anything worth catching. Read it as a trend across runs.

## Comparing fairly

The workflow builds stock Firefox from `FIREFOX_UPSTREAM_COMMIT` in
`.PLAYWRIGHT_VERSION` with a mozconfig matching Foxhound's, so the two differ
in the instrumentation and not in build options.

Do not substitute a Firefox downloaded from mozilla.org. Release builds are
built with PGO and LTO that these builds are not, so the comparison would
attribute the difference in build configuration to taint tracking and overstate
the overhead considerably.

## What has been measured

At the time this was written, taint tracking cost roughly 17 to 18% on
SunSpider against a stock Firefox built from the merge base. Most of that is
structural rather than propagation logic: `JSString` grows from 24 to 40 bytes
to carry the taint pointer and the inline character padding that comes with it.
