/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/TaintGuards.h"

#include "mozilla/Sprintf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jit/MacroAssembler.h"
#include "threading/Thread.h"
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

static void WriteTaintGuardCounts(FILE* out) {
  for (size_t i = 0; i < size_t(TaintGuard::Count); i++) {
    fprintf(out, "[taint-guard] pid=%d %s %llu\n", int(getpid()),
            sTaintGuardNames[i], (unsigned long long)sTaintGuardHits[i]);
  }
}

static void DumpTaintGuardCounts() { WriteTaintGuardCounts(stderr); }

// Content processes end with an immediate exit that skips atexit, so in
// directory mode the counts are rewritten to <dir>/taint-guards.<pid> once a
// second instead.
static void WriteTaintGuardCountsLoop(const char* dir) {
  char path[1024];
  char tmp[1040];
  SprintfLiteral(path, "%s/taint-guards.%d", dir, int(getpid()));
  SprintfLiteral(tmp, "%s.tmp", path);
  while (true) {
    ThisThread::SleepMilliseconds(1000);
    if (FILE* out = fopen(tmp, "w")) {
      WriteTaintGuardCounts(out);
      fclose(out);
      rename(tmp, path);
    }
  }
}

bool js::jit::TaintGuardCountsEnabled() {
  static const bool enabled = [] {
    const char* value = getenv("FOXHOUND_TAINT_GUARD_COUNTS");
    if (!value || !*value) {
      return false;
    }
    if (strcmp(value, "1") == 0) {
      atexit(DumpTaintGuardCounts);
      return true;
    }
    Thread writer;
    if (writer.init(WriteTaintGuardCountsLoop, value)) {
      writer.detach();
    }
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
