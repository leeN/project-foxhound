/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Warp compiled single character reads into char code arithmetic, which
// carries no taint. Each call site is warmed on untainted input first, as a
// crawled page would, so the inline path is in place when tainted input
// arrives. Run this under every tier configuration:
// jstests.py --jitflags=all non262/taint

var WARMUP = 3000;

function checkHot(cases, warmInput, input) {
  for (var [f, op] of cases) {
    for (var i = 0; i < WARMUP; i++)
      f(warmInput);
    for (var i = 0; i < 100; i++) {
      var res = f(taint(input));
      assertFullTainted(res);
      if (op)
        assertLastTaintOperationEquals(res, op);
    }
  }
}

function charAccessTest() {
  checkHot([
    [s => s.charAt(1), "charAt"],
    [s => s[1], null],
    [s => s.charAt(0).toUpperCase(), "toUpperCase"],
  ], "untainted", "tainted");
  checkHot([
    [s => s[0].toLowerCase(), "toLowerCase"],
  ], "UNTAINTED", "TAINTED");
}

// MSubstr folded these single character substrings into char access.
function singleCharSubstringTest() {
  checkHot([
    [s => s.substring(0, 1), "substring"],
    [s => s.slice(-1), "slice"],
    [s => s.substr(-1), "substr"],
  ], "untainted", "tainted");
}

charAccessTest();
singleCharSubstringTest();

if (typeof reportCompare === "function")
  reportCompare(true, true);
