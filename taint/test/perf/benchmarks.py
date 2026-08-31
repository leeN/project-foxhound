#!/usr/bin/env python3
"""Definitions for the browser benchmarks used to measure taint overhead.

Each benchmark is a directory of static files somewhere in the tree plus a
driver page. The drivers all report their results to the page in their own way,
so each one gets a small patch that makes it hand the results back to the local
harness instead. Nothing outside the tree is downloaded.
"""

import json
import os
import re

# Where each benchmark lives, relative to the top of the source tree.
SUNSPIDER = "third_party/webkit/PerformanceTests/SunSpider/sunspider-1.0.1/sunspider-1.0.1"
KRAKEN = "testing/talos/talos/tests/kraken"
V8 = "testing/talos/talos/tests/v8_7"
DROMAEO = "testing/talos/talos/tests/dromaeo"
SELECTORS = "taint/test/perf/selectors"


def _patch_location_driver(html):
    """SunSpider and Kraken both fall back to navigating to results.html with
    the result JSON in the query string when the Talos hooks are absent. Point
    that at the harness instead."""
    patched = html.replace(
        'location = "results.html?" + encodeURI(outputString);',
        'location = "/report?" + encodeURI(outputString);',
    )
    if "/report?" not in patched:
        raise RuntimeError("could not find the results.html redirect to patch")
    return patched


def patch_sunspider(html):
    html = _patch_location_driver(html)
    # The driver only auto-starts under ?raptor; start unconditionally.
    return html.replace("if (raptorMode) {\n    start();\n}", "start();")


# Kraken's test content reads TalosContentProfiler off the parent window and
# chains .then() onto subtestStart/subtestEnd, so without Talos those calls
# throw and the test never reports a result. Swap the unavailable talos-powers
# script for a stub that resolves, in the same place the driver loads it.
KRAKEN_TALOS_SCRIPT = (
    '<script src="resource://talos-powers/TalosContentProfiler.js"></script>'
)
KRAKEN_TALOS_STUB = (
    "<script>window.TalosContentProfiler={"
    "subtestStart:function(){return Promise.resolve();},"
    "subtestEnd:function(){return Promise.resolve();},"
    "resume:function(){return Promise.resolve();},"
    "pause:function(){return Promise.resolve();},"
    "beginTest:function(){return Promise.resolve();},"
    "finishTest:function(){return Promise.resolve();}};</script>"
)


def patch_kraken(html):
    # Kraken already starts from <body onload>.
    html = _patch_location_driver(html)
    if KRAKEN_TALOS_SCRIPT not in html:
        raise RuntimeError("could not find the talos-powers script tag to stub")
    return html.replace(KRAKEN_TALOS_SCRIPT, KRAKEN_TALOS_STUB)


# base.js calls Profiler.subtestStart/subtestEnd around every benchmark. The
# Profiler script lives outside the benchmark directory, so it is not there
# when the benchmark is staged on its own and every benchmark fails. Stub it in
# place of the script tag, the way Kraken's profiler is stubbed.
V8_PROFILER_SCRIPT = '<script src="../../scripts/Profiler.js"></script>'
V8_PROFILER_STUB = (
    "<script>window.Profiler={"
    "subtestStart:function(){},subtestEnd:function(){},"
    "resume:function(){},pause:function(){},"
    "beginTest:function(){},finishTest:function(){}};</script>"
)


def patch_v8(html):
    if V8_PROFILER_SCRIPT not in html:
        raise RuntimeError("could not find the Profiler script tag to stub")
    html = html.replace(V8_PROFILER_SCRIPT, V8_PROFILER_STUB)
    # V8 reports by writing into the DOM rather than navigating. Wrap the
    # notification callbacks before Run() reads them, collect the per-benchmark
    # numbers and the final score, then hand them back the same way.
    shim = """
<script>
(function () {
  var collected = {};
  var origAddResult = AddResult;
  AddResult = function (name, result) {
    collected[name] = result;
    return origAddResult(name, result);
  };
  var origAddScore = AddScore;
  AddScore = function (score) {
    origAddScore(score);
    collected["Score"] = score;
    location = "/report?" + encodeURI(JSON.stringify(collected));
  };
})();
</script>
</body>"""
    if "</body>" not in html:
        raise RuntimeError("could not find </body> to insert the V8 shim")
    return html.replace("</body>", shim, 1)


def parse_keyed_lists(raw):
    """SunSpider and Kraken hand back {"test": [t1, t2, ...], ...} in ms."""
    data = json.loads(raw)
    data.pop("v", None)
    return {k: [float(x) for x in v] for k, v in data.items()}


def parse_v8(raw):
    """V8 hands back {"Richards": "812", ..., "Score": "947"}; higher is
    better. Values are plain numbers rendered as strings."""
    data = json.loads(raw)
    out = {}
    for key, value in data.items():
        match = re.search(r"[-+]?\d*\.?\d+", str(value))
        if match:
            out[key] = [float(match.group())]
    if not out:
        raise RuntimeError("no numeric results in the V8 payload")
    return out


# Dromaeo's Talos wrapper pages report through tpRecordTime, which is handed a
# comma separated list of raw numbers with no subtest names attached, while the
# named per-subtest results stay in a closure in webrunner.js. Hook the runner
# where it already summarises a finished subtest, so the names come back with
# the numbers, and let the driver hand the collection over when Talos would have
# been called.
DROMAEO_LOGTEST = "\tfunction logTest(data){\n"
DROMAEO_LOGTEST_HOOK = (
    "\tfunction logTest(data){\n"
    "\t\t(window.__dromaeoResults = window.__dromaeoResults || []).push(\n"
    "\t\t    {name: data.name, mean: parseFloat(data.mean)});\n"
)

DROMAEO_RUNNER_SCRIPT = '<script src="webrunner.js"></script>'
# encodeURIComponent, not encodeURI: Dromaeo's CSS selector suites name sub-tests
# after the selector they run, and five of them contain a "#". encodeURI leaves
# that alone, the browser reads it as the start of a fragment, and the payload is
# cut off mid-string.
DROMAEO_REPORT_STUB = (
    '<script src="webrunner.js"></script>\n'
    "<script>window.tpRecordTime=function(){"
    'location="/report?"'
    "+encodeURIComponent(JSON.stringify(window.__dromaeoResults||[]));"
    "};</script>"
)


def patch_dromaeo_webrunner(js):
    if DROMAEO_LOGTEST not in js:
        raise RuntimeError("could not find Dromaeo's logTest to hook")
    return js.replace(DROMAEO_LOGTEST, DROMAEO_LOGTEST_HOOK, 1)


def patch_dromaeo(html):
    # Dromaeo reads TalosContentProfiler the way Kraken does, off a resource://
    # script that is not there outside Talos.
    if KRAKEN_TALOS_SCRIPT not in html:
        raise RuntimeError("could not find the talos-powers script tag to stub")
    html = html.replace(KRAKEN_TALOS_SCRIPT, KRAKEN_TALOS_STUB)
    if DROMAEO_RUNNER_SCRIPT not in html:
        raise RuntimeError("could not find the webrunner script tag")
    return html.replace(DROMAEO_RUNNER_SCRIPT, DROMAEO_REPORT_STUB, 1)


def parse_dromaeo(raw):
    """Dromaeo hands back [{"name": ..., "mean": ...}, ...] in runs/s, one entry
    per run of each subtest; higher is better."""
    out = {}
    for entry in json.loads(raw):
        out.setdefault(entry["name"], []).append(float(entry["mean"]))
    if not out:
        raise RuntimeError("no Dromaeo subtest results in the payload")
    return out


def patch_none(html):
    """For benchmarks written for this harness, which report to /report already."""
    return html


BENCHMARKS = {
    "sunspider": {
        "path": SUNSPIDER,
        "driver": "driver.html",
        "patch": patch_sunspider,
        "parse": parse_keyed_lists,
        "unit": "ms",
        "higher_is_better": False,
        "label": "SunSpider 1.0.1",
    },
    "kraken": {
        "path": KRAKEN,
        "driver": "driver.html",
        "patch": patch_kraken,
        "parse": parse_keyed_lists,
        "unit": "ms",
        "higher_is_better": False,
        "label": "Kraken 1.1",
    },
    "v8": {
        "path": V8,
        "driver": "run.html",
        "patch": patch_v8,
        "parse": parse_v8,
        "unit": "score",
        "higher_is_better": True,
        "label": "V8 benchmark v7",
    },
    # The three above are pure JavaScript and never call a selector API, so they
    # cannot see a change to one. These do.
    "dromaeo-dom-query": {
        "path": DROMAEO,
        "driver": "dom-query.html",
        "patch": patch_dromaeo,
        "patch_files": {"webrunner.js": patch_dromaeo_webrunner},
        "parse": parse_dromaeo,
        "unit": "runs/s",
        "higher_is_better": True,
        "label": "Dromaeo DOM query",
    },
    "dromaeo-cssquery-jquery": {
        "path": DROMAEO,
        "driver": "cssquery-jquery.html",
        "patch": patch_dromaeo,
        "patch_files": {"webrunner.js": patch_dromaeo_webrunner},
        "parse": parse_dromaeo,
        "unit": "runs/s",
        "higher_is_better": True,
        "label": "Dromaeo CSS selectors (jQuery)",
    },
    "selectors": {
        "path": SELECTORS,
        "driver": "driver.html",
        "patch": patch_none,
        "parse": parse_keyed_lists,
        "unit": "ms",
        "higher_is_better": False,
        "label": "Selector microbenchmark",
    },
}


def resolve(topsrcdir, name):
    spec = dict(BENCHMARKS[name])
    spec["name"] = name
    spec["abspath"] = os.path.join(topsrcdir, spec["path"])
    return spec
