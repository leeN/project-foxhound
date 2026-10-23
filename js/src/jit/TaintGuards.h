/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_TaintGuards_h
#define jit_TaintGuards_h

#include <stdint.h>

#include "jit/Registers.h"

namespace js {
namespace jit {

class Label;
class MacroAssembler;

// Foxhound: every JIT fast path that reads or builds string contents leaves
// tainted strings to a slow path. Each such check is one of these.
#define FOR_EACH_TAINT_GUARD(_) \
  _(ICLoadStringChar)           \
  _(ICLoadStringCharCode)       \
  _(RegExpMatchSubstring)       \
  _(Substr)                     \
  _(Concat)                     \
  _(LinearizeString)            \
  _(CharCodeAt)                 \
  _(ToLowerCase)                \
  _(AtomizeSlot)                \
  _(GuardStringUntainted)

enum class TaintGuard : uint8_t {
#define DEFINE_GUARD(name) name,
  FOR_EACH_TAINT_GUARD(DEFINE_GUARD)
#undef DEFINE_GUARD
      Count
};

// Counting is enabled by setting FOXHOUND_TAINT_GUARD_COUNTS in the
// environment. The counts are written to stderr when the process exits.
// Without it the emitted code is a single branch.
bool TaintGuardCountsEnabled();

// Jump to |label| when |str| is tainted.
void BranchIfStringTainted(MacroAssembler& masm, Register str, Label* label,
                           TaintGuard guard);

// Jump to |label| when |taint|, a loaded taint pointer (or several or'ed
// together), is non-null.
void BranchIfTaintNonNull(MacroAssembler& masm, Register taint, Label* label,
                          TaintGuard guard);

}  // namespace jit
}  // namespace js

#endif /* jit_TaintGuards_h */
