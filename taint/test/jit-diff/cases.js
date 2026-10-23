/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Cases for driver.js. Each expression reads its input from `s`. `inputs`
// restricts a case to some of the driver's input kinds.

var CASES = [
  // Concatenation
  { id: "concat-right", expr: 's + "x"' },
  { id: "concat-left", expr: '"x" + s' },
  { id: "concat-self", expr: "s + s" },
  { id: "concat-num", expr: "s + 1" },
  { id: "concat-assign", expr: '(s += "x", s)' },
  { id: "concat-chain", expr: '"a" + s + "b" + s + "c"' },
  { id: "template", expr: "`a${s}b`" },
  { id: "template-only", expr: "`${s}`" },
  { id: "concat-method", expr: 's.concat("x", s)' },
  { id: "concat-empty", expr: 's + ""' },
  { id: "concat-empty-left", expr: '"" + s' },
  { id: "concat-object", expr: 'new String(s) + "x"' },

  // Substrings
  { id: "substring-1", expr: "s.substring(1)" },
  { id: "substring-1-4", expr: "s.substring(1, 4)" },
  { id: "substring-0-1", expr: "s.substring(0, 1)" },
  { id: "substring-full", expr: "s.substring(0)" },
  { id: "slice-1", expr: "s.slice(1)" },
  { id: "slice-neg", expr: "s.slice(-3)" },
  { id: "slice-neg1", expr: "s.slice(-1)" },
  { id: "slice-1-neg1", expr: "s.slice(1, -1)" },
  { id: "substr-2-3", expr: "s.substr(2, 3)" },
  { id: "substr-neg1", expr: "s.substr(-1)" },
  { id: "substring-twice", expr: "s.substring(1).substring(1)" },

  // Character access
  { id: "index-0", expr: "s[0]" },
  { id: "index-1", expr: "s[1]" },
  { id: "index-last", expr: "s[s.length - 1]" },
  { id: "index-oob", expr: "s[100]" },
  { id: "charAt-0", expr: "s.charAt(0)" },
  { id: "charAt-2", expr: "s.charAt(2)" },
  { id: "at-0", expr: "s.at(0)" },
  { id: "at-neg1", expr: "s.at(-1)" },
  { id: "charCodeAt", expr: "s.charCodeAt(1)" },
  { id: "codePointAt", expr: "s.codePointAt(1)" },
  { id: "fromCharCode-roundtrip", expr: "String.fromCharCode(s.charCodeAt(1))" },
  { id: "fromCodePoint-roundtrip", expr: "String.fromCodePoint(s.codePointAt(1))" },
  { id: "char-concat", expr: "s[0] + s[1]" },
  { id: "charAt-concat", expr: "s.charAt(0) + s.charAt(1)" },
  { id: "charAt-upper", expr: "s.charAt(0).toUpperCase()" },
  { id: "spread", expr: "[...s]" },
  { id: "for-of", expr: "(function(){ var r = []; for (var c of s) r.push(c); return r; })()" },

  // Split and join
  { id: "split-empty", expr: 's.split("")' },
  { id: "split-char", expr: 's.split("b")' },
  { id: "split-regexp", expr: "s.split(/c/)" },
  { id: "split-join", expr: 's.split("b").join("B")' },
  { id: "join", expr: '[s, "x"].join("-")' },
  { id: "join-single", expr: "[s].join()" },
  { id: "join-sep", expr: '["a", "b"].join(s)' },
  { id: "array-tostring", expr: '[s, "x"] + ""' },

  // Replace
  { id: "replace-str", expr: 's.replace("bc", "Z")' },
  { id: "replace-str-tainted", expr: 's.replace("bc", s)' },
  { id: "replace-str-pattern", expr: 's.replace("bc", "[$&]")' },
  { id: "replace-re", expr: 's.replace(/bc/, "Z")' },
  { id: "replace-re-g", expr: 's.replace(/bc/g, "Z")' },
  { id: "replace-re-groups", expr: 's.replace(/b(c)/g, "[$&$1]")' },
  { id: "replace-fn", expr: 's.replace("bc", function(m) { return m + "!"; })' },
  { id: "replace-re-fn", expr: "s.replace(/b(c)/g, function(m, p) { return p + m; })" },
  { id: "replaceAll-str", expr: 's.replaceAll("bc", "Z")' },
  { id: "replaceAll-re", expr: 's.replaceAll(/bc/g, "Z")' },

  // Case, trim, pad, repeat, normalize
  { id: "lower", expr: "s.toLowerCase()" },
  { id: "upper", expr: "s.toUpperCase()" },
  { id: "lower-upper", expr: "s.toLowerCase().toUpperCase()" },
  { id: "locale-lower", expr: "s.toLocaleLowerCase()" },
  { id: "trim", expr: '(" " + s + " ").trim()' },
  { id: "trim-noop", expr: "s.trim()" },
  { id: "trimStart", expr: '(" " + s).trimStart()' },
  { id: "trimEnd", expr: '(s + " ").trimEnd()' },
  { id: "padStart", expr: 's.padStart(40, "-")' },
  { id: "padEnd", expr: 's.padEnd(40, "-")' },
  { id: "repeat", expr: "s.repeat(3)" },
  { id: "normalize", expr: 's.normalize("NFD")' },

  // Conversions
  { id: "String", expr: "String(s)" },
  { id: "toString", expr: "s.toString()" },
  { id: "valueOf", expr: "s.valueOf()" },
  { id: "string-object", expr: '"" + new String(s)' },

  // JSON and URI
  { id: "json-stringify", expr: "JSON.stringify(s)" },
  { id: "json-stringify-obj", expr: "JSON.stringify({ k: s })" },
  { id: "json-roundtrip", expr: "JSON.parse(JSON.stringify({ k: s })).k" },
  { id: "encodeURIComponent", expr: "encodeURIComponent(s)" },
  { id: "decodeURIComponent", expr: "decodeURIComponent(s)" },
  { id: "encodeURI", expr: "encodeURI(s)" },
  { id: "escape", expr: "escape(s)" },
  { id: "unescape", expr: "unescape(s)" },

  // RegExp
  { id: "exec", expr: "/b(c)/.exec(s)" },
  { id: "exec-g", expr: "/b(c)/g.exec(s)" },
  { id: "exec-named", expr: "/(?<n>b)c/.exec(s)" },
  { id: "exec-nomatch", expr: "/x/.exec(s)" },
  { id: "match", expr: "s.match(/b(c)/)" },
  { id: "match-g", expr: "s.match(/bc/g)" },
  { id: "match-str", expr: 's.match("bc")' },
  { id: "matchAll", expr: "[...s.matchAll(/b(c)/g)]" },
  { id: "test", expr: "/bc/.test(s)" },
  { id: "search", expr: "s.search(/c/)" },

  // Property keys and values
  { id: "key", expr: "Object.keys({ [s]: 1 })[0]" },
  { id: "key-short", expr: "Object.keys({ [s.slice(0, 2)]: 1 })[0]" },
  { id: "array-element", expr: "[s][0]" },
  { id: "object-value", expr: "({ a: s }).a" },
  { id: "map", expr: "[s].map(x => x)[0]" },
  { id: "conditional", expr: 's ? s : "x"' },
];
