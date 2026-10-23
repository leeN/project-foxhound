/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/TaintGuards.h"

#include <stdio.h>
#include <stdlib.h>

#include "jit/MacroAssembler.h"
#include "util/GetPidProvider.h"
#include "vm/StringType.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;

static uint64_t sTaintGuardHits[size_t(TaintGuard::Count)];

static const char* const sTaintGuardNames[] = {
#define GUARD_NAME(name) #name,
    FOR_EACH_TAINT_GUARD(GUARD_NAME)
#undef GUARD_NAME
};

static void DumpTaintGuardCounts() {
  for (size_t i = 0; i < size_t(TaintGuard::Count); i++) {
    fprintf(stderr, "[taint-guard] pid=%d %s %llu\n", int(getpid()),
            sTaintGuardNames[i], (unsigned long long)sTaintGuardHits[i]);
  }
}

bool js::jit::TaintGuardCountsEnabled() {
  static const bool enabled = [] {
    if (!getenv("FOXHOUND_TAINT_GUARD_COUNTS")) {
      return false;
    }
    atexit(DumpTaintGuardCounts);
    return true;
  }();
  return enabled;
}

static void CountAndJump(MacroAssembler& masm, Label* label, TaintGuard guard) {
  masm.inc64(AbsoluteAddress(&sTaintGuardHits[size_t(guard)]));
  masm.jump(label);
}

void js::jit::BranchIfStringTainted(MacroAssembler& masm, Register str,
                                    Label* label, TaintGuard guard) {
  Address taint(str, JSString::offsetOfTaint());
  if (!TaintGuardCountsEnabled()) {
    masm.branchPtr(Assembler::NotEqual, taint, ImmPtr(nullptr), label);
    return;
  }
  Label untainted;
  masm.branchPtr(Assembler::Equal, taint, ImmPtr(nullptr), &untainted);
  CountAndJump(masm, label, guard);
  masm.bind(&untainted);
}

void js::jit::BranchIfTaintNonNull(MacroAssembler& masm, Register taint,
                                   Label* label, TaintGuard guard) {
  if (!TaintGuardCountsEnabled()) {
    masm.branchTestPtr(Assembler::NonZero, taint, taint, label);
    return;
  }
  Label untainted;
  masm.branchTestPtr(Assembler::Zero, taint, taint, &untainted);
  CountAndJump(masm, label, guard);
  masm.bind(&untainted);
}
