/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// replace and replaceAll record their own operation on the result. The
// concatenations their implementation performs to assemble the result must
// not appear in the flow. Concatenations the page performs, including inside a
// replacement callback, must.

var WARMUP = 3000;

function opNames(str) {
  return str.taint.map(r => r.flow.map(op => op.operation)
                                   .filter(name => name !== "function")
                                   .reverse());
}

function ranges(str) {
  return JSON.stringify(str.taint.map(r => [r.begin, r.end]));
}

function check(f, input, expected, expectedRanges, expectedOps) {
  for (var i = 0; i < WARMUP; i++)
    f("untainted " + input);
  for (var i = 0; i < 100; i++) {
    var res = f(taint(input));
    assertEq(res, expected);
    assertEq(ranges(res), expectedRanges);
    for (var ops of opNames(res))
      assertEq(JSON.stringify(ops), JSON.stringify(expectedOps));
  }
}

var REPLACE = ["manual taint source", "replace"];
var REPLACE_ALL = ["manual taint source", "replaceAll"];

check(s => s.replace(/\n/g, " "), "<p>x</p>\n", "<p>x</p> ",
      "[[0,8]]", REPLACE);
check(s => s.replace(/x/g, "yy"), "axbxc", "ayybyyc",
      "[[0,1],[3,4],[6,7]]", REPLACE);
check(s => s.replace(/^<svg/i, "<svg a"), "<svg x>", "<svg a x>",
      "[[6,9]]", REPLACE);
check(s => s.replace(/b/, "-"), "abc", "a-c", "[[0,1],[2,3]]", REPLACE);
check(s => s.replace(/(b)/g, "[$1]"), "abc", "a[b]c",
      "[[0,1],[2,3],[4,5]]", REPLACE);
check(s => s.replace(/(b)/, "[$1]"), "abc", "a[b]c",
      "[[0,1],[2,3],[4,5]]", REPLACE);
check(s => s.replace(/b/g, () => "-"), "abc", "a-c", "[[0,1],[2,3]]",
      REPLACE);
check(s => s.replace(/(b)/g, () => "-"), "abc", "a-c", "[[0,1],[2,3]]",
      REPLACE);
check(s => s.replace(/b/, () => "-"), "abc", "a-c", "[[0,1],[2,3]]",
      REPLACE);
check(s => s.replace("b", () => "-"), "abc", "a-c", "[[0,1],[2,3]]",
      REPLACE);
check(s => s.replaceAll(/b/g, "-"), "abc", "a-c", "[[0,1],[2,3]]",
      REPLACE_ALL);
check(s => s.replaceAll("b", () => "-"), "abc", "a-c", "[[0,1],[2,3]]",
      REPLACE_ALL);

// The page's own concatenation before the call is still recorded.
check(s => ("pre" + s).replace(/x/g, "y"), "axb", "preayb",
      "[[3,4],[5,6]]", ["manual taint source", "concat", "replace"]);

// A concatenation inside the replacement callback is page code.
(function () {
  function f(s) { return s.replace(/b/g, m => m + "!"); }
  for (var i = 0; i < WARMUP; i++)
    f("untainted abc");
  for (var i = 0; i < 100; i++) {
    var res = f(taint("abc"));
    assertEq(res, "ab!c");
    assertEq(ranges(res), "[[0,1],[1,2],[3,4]]");
    assertEq(JSON.stringify(opNames(res)), JSON.stringify([
      ["manual taint source", "replace"],
      ["manual taint source", "concat", "replace"],
      ["manual taint source", "replace"],
    ]));
  }
})();

// match and split record their own operation, not the exec they run. exec and
// matchAll, whose results the page sees directly, still record exec.
function checkArray(f, input, index, expected, expectedOps) {
  for (var i = 0; i < WARMUP; i++)
    f("untainted " + input);
  for (var i = 0; i < 100; i++) {
    var res = f(taint(input))[index];
    assertEq(res, expected);
    for (var ops of opNames(res))
      assertEq(JSON.stringify(ops), JSON.stringify(expectedOps));
  }
}

var MATCH = ["manual taint source", "match"];
var EXEC = ["manual taint source", "RegExp.prototype.exec"];

checkArray(s => s.match(/^<div\s+(.*?)>/i), "<div a=\"1\"></div>", 1,
           "a=\"1\"", MATCH);
checkArray(s => s.match(/a./g), "a1a2", 1, "a2", MATCH);
checkArray(s => s.split(/(,)/), "a,b", 2, "b",
           ["manual taint source", "split"]);
checkArray(s => /a(.)/.exec(s), "xa1", 1, "1", EXEC);
checkArray(s => [...s.matchAll(/a(.)/g)][1], "a1a2", 1, "2", EXEC);

// Calling @@replace and @@match directly records the operation once.
check(s => /b/g[Symbol.replace](s, "-"), "abc", "a-c", "[[0,1],[2,3]]",
      REPLACE);
checkArray(s => /a(.)/[Symbol.match](s), "xa1", 1, "1", MATCH);

// A RegExp with an own property is not optimizable, so the String methods
// look up the builtin @@replace / @@match. The operation is still recorded
// once, under the name of the method the page called.
function unoptimizable(re) {
  re.extra = 1;
  return re;
}
check(s => s.replace(unoptimizable(/b/g), "-"), "abc", "a-c",
      "[[0,1],[2,3]]", REPLACE);
check(s => s.replaceAll(unoptimizable(/b/g), "-"), "abc", "a-c",
      "[[0,1],[2,3]]", REPLACE_ALL);
checkArray(s => s.match(unoptimizable(/a(.)/)), "xa1", 1, "1", MATCH);

// A page-defined @@replace is page code: its result still gets the replace
// operation from String.prototype.replace.
(function () {
  var custom = { [Symbol.replace](s, r) { return s.toUpperCase(); } };
  var res = taint("abc").replace(custom, "-");
  assertEq(res, "ABC");
  assertEq(JSON.stringify(opNames(res)[0]),
           JSON.stringify(["manual taint source", "toUpperCase", "replace"]));
})();

if (typeof reportCompare === "function")
  reportCompare(true, true);
