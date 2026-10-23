/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Differential taint driver. Runs every case in cases.js under four access
// patterns and prints one JSON line per (case, input, pattern) with the
// canonical taint of the results. run.py compares the output of an
// interpreter-only shell against JIT configurations.
//
// Usage: js [flags] driver.js DIR SHARD NSHARDS [FILTER] [SCALE]

var __td_dir = scriptArgs[0];
var __td_shard = +scriptArgs[1];
var __td_nshards = +scriptArgs[2];
var __td_filter = scriptArgs[3] ? new RegExp(scriptArgs[3]) : null;
var __td_scale = scriptArgs[4] ? +scriptArgs[4] : 1;

load(__td_dir + "/cases.js");

// Same iteration counts in every configuration, high enough for the default
// warmup thresholds to reach Ion.
var __td_hot = Math.max(4, Math.round(2000 * __td_scale));
var __td_after = 50;

var __td_T = String.tainted;

// Each input kind returns [untainted, tainted] with identical characters.
var INPUTS = {
  lat1: () => ["aBc-bc bc%41<b>", __td_T("aBc-bc bc%41<b>")],
  twobyte: () => ["aBc-bc bc%41<b>ςİ", __td_T("aBc-bc bc%41<b>ςİ")],
  partial: () => ["pre" + "aBc-bc bc%41<b>" + "post", "pre" + __td_T("aBc-bc bc%41<b>") + "post"],
  rope: () => ["aBc-bc bc%41<b>" + "................................", __td_T("aBc-bc bc%41<b>") + "................................"],
  long: () => ["aBc-bc bc%41<b>".repeat(8), __td_T("aBc-bc bc%41<b>".repeat(8))],
  tiny: () => ["bc", __td_T("bc")],
};

function __td_basename(s) {
  return s.replace(/[^\s"']*\/([^/\s"']+)/g, "$1");
}

function __td_isDriverOp(op) {
  return op.operation === "function" &&
    (/^__td_/.test(op.arguments[0]) || /^driver\.js:\d+$/.test(__td_basename(op.arguments[1])));
}

function __td_flow(str) {
  return str.taint.map(r => ({
    b: r.begin,
    e: r.end,
    f: r.flow.filter(op => !__td_isDriverOp(op)).map(op => ({
      op: op.operation,
      args: op.arguments.map(a => __td_basename(String(a)).slice(0, 200)),
      fn: op.location.function,
      line: op.location.line,
    })),
  }));
}

function __td_snap(v, depth) {
  depth = depth || 0;
  if (depth > 3) {
    return "<deep>";
  }
  if (typeof v === "string") {
    return { s: v.length > 300 ? v.slice(0, 300) + "...(" + v.length + ")" : v, t: __td_flow(v) };
  }
  if (v === null || (typeof v !== "object" && typeof v !== "function")) {
    return typeof v + ":" + String(v);
  }
  if (typeof v === "function") {
    return "function";
  }
  if (v.__td_throw) {
    return "throw:" + v.__td_throw;
  }
  if (Array.isArray(v)) {
    var o = { a: v.map(e => __td_snap(e, depth + 1)) };
    if ("index" in v) {
      o.index = v.index;
    }
    if ("input" in v) {
      o.input = __td_snap(v.input, depth + 1);
    }
    if (v.groups) {
      o.groups = __td_snap(Object.assign({}, v.groups), depth + 1);
    }
    return o;
  }
  var r = {};
  for (var k of Object.keys(v)) {
    r[k] = __td_snap(v[k], depth + 1);
  }
  return r;
}

function __td_compile(expr) {
  return (0, eval)("(function __td_case(s) {\n  return (" + expr + ");\n})");
}

function __td_compileLoop(expr) {
  return (0, eval)(
    "(function __td_loop(u, t, K, N) {\n" +
    "  var first, r;\n" +
    "  for (var i = 0; i < N; i++) {\n" +
    "    var s = i < K ? u : t;\n" +
    "    r = (" + expr + ");\n" +
    "    if (i === K) first = r;\n" +
    "  }\n" +
    "  return [first, r];\n" +
    "})");
}

function __td_call(f, s) {
  try {
    return f(s);
  } catch (e) {
    return { __td_throw: String(e) };
  }
}

// Each pattern gets a fresh function, so its inline caches start empty.
var PATTERNS = {
  // Tainted from the first call.
  tainted(expr, u, t) {
    var f = __td_compile(expr);
    var first = __td_call(f, t);
    var last;
    for (var i = 0; i < __td_hot; i++) {
      last = __td_call(f, t);
    }
    return { first: __td_snap(first), last: __td_snap(last) };
  },
  // Warmed up on untainted input, then switched to tainted.
  switch(expr, u, t) {
    var f = __td_compile(expr);
    for (var i = 0; i < __td_hot; i++) {
      __td_call(f, u);
    }
    var first = __td_call(f, t);
    var last;
    for (var i = 0; i < __td_after; i++) {
      last = __td_call(f, t);
    }
    return { first: __td_snap(first), last: __td_snap(last) };
  },
  // Tainted and untainted alternating.
  alt(expr, u, t) {
    var f = __td_compile(expr);
    var lu, lt;
    for (var i = 0; i < __td_hot; i++) {
      if (i & 1) {
        lt = __td_call(f, t);
      } else {
        lu = __td_call(f, u);
      }
    }
    return { untainted: __td_snap(lu), tainted: __td_snap(lt) };
  },
  // Switched to tainted inside a hot loop, after an OSR compile.
  loop(expr, u, t) {
    var f = __td_compileLoop(expr);
    var res;
    try {
      res = f(u, t, __td_hot, __td_hot + __td_after);
    } catch (e) {
      return { throw: String(e) };
    }
    return { first: __td_snap(res[0]), last: __td_snap(res[1]) };
  },
};

var __td_index = 0;
for (var c of CASES) {
  for (var kind of c.inputs || Object.keys(INPUTS)) {
    for (var pat of Object.keys(PATTERNS)) {
      var id = c.id + "/" + kind + "/" + pat;
      if (__td_index++ % __td_nshards !== __td_shard) {
        continue;
      }
      if (__td_filter && !__td_filter.test(id)) {
        continue;
      }
      var [u, t] = INPUTS[kind]();
      var fresh = __td_snap(t);
      var out = PATTERNS[pat](c.expr, u, t);
      var after = __td_snap(t);
      var line = { id, expr: c.expr, out };
      if (JSON.stringify(after) !== JSON.stringify(fresh)) {
        line.input = after;
      }
      print(JSON.stringify(line));
    }
  }
}
