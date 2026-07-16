/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Foxhound: IPC ParamTraits for the taint data structures.
 *
 * These allow a StringTaint (and its whole taint-flow graph) to be sent
 * *directly* over IPC as a native message parameter, instead of being
 * serialized to a JSON string first. They mirror the JSON (de)serialization in
 * Taint.cpp (Dump*AsJSON / Load*FromJSON) exactly, but write the data using the
 * IPC MessageWriter/MessageReader primitives.
 *
 * The main consumer is ParamTraits<nsTSubstring<T>> in
 * IPCMessageUtilsSpecializations.h, which uses these to carry each string's
 * taint alongside its characters. As a result any tainted nsCString/nsString
 * keeps its taint when it crosses a process boundary.
 */

#ifndef _TaintIPCUtils_h
#define _TaintIPCUtils_h

#include <string>
#include <vector>

#include "chrome/common/ipc_message_utils.h"
#include "mozilla/CheckedInt.h"

#include "Taint.h"

namespace IPC {

namespace taint_detail {

// std::string and std::u16string have no built-in ParamTraits, and we
// deliberately avoid reusing the ns*String traits here: those now also carry
// taint (see ParamTraits<nsTSubstring>), which would be wasteful noise on the
// operation names, arguments and locations that make up a taint flow. Instead
// we serialize the raw character buffers with a length prefix.
template <typename CharT>
void WriteStdString(MessageWriter* aWriter,
                    const std::basic_string<CharT>& aStr) {
  WriteParam(aWriter, static_cast<uint32_t>(aStr.length()));
  if (!aStr.empty()) {
    aWriter->WriteBytes(aStr.data(), aStr.length() * sizeof(CharT));
  }
}

template <typename CharT>
bool ReadStdString(MessageReader* aReader, std::basic_string<CharT>* aStr) {
  uint32_t length = 0;
  if (!ReadParam(aReader, &length)) {
    return false;
  }
  if (length == 0) {
    aStr->clear();
    return true;
  }
  mozilla::CheckedInt<uint32_t> byteLength =
      mozilla::CheckedInt<uint32_t>(length) * sizeof(CharT);
  if (!byteLength.isValid()) {
    return false;
  }
  aStr->resize(length);
  return aReader->ReadBytesInto(&(*aStr)[0], byteLength.value());
}

}  // namespace taint_detail

// std::array<unsigned char, 16>, i.e. TaintMd5.
template <>
struct ParamTraits<TaintMd5> {
  static void Write(MessageWriter* aWriter, const TaintMd5& aParam) {
    aWriter->WriteBytes(aParam.data(), aParam.size());
  }
  static bool Read(MessageReader* aReader, TaintMd5* aResult) {
    return aReader->ReadBytesInto(aResult->data(), aResult->size());
  }
};

// Mirrors Dump/LoadTaintLocationFromJSON.
template <>
struct ParamTraits<TaintLocation> {
  static void Write(MessageWriter* aWriter, const TaintLocation& aParam) {
    taint_detail::WriteStdString(aWriter, aParam.filename());
    WriteParam(aWriter, aParam.line());
    WriteParam(aWriter, aParam.pos());
    WriteParam(aWriter, aParam.next_line());
    WriteParam(aWriter, aParam.next_pos());
    WriteParam(aWriter, aParam.scriptStartLine());
    WriteParam(aWriter, aParam.scriptHash());
    taint_detail::WriteStdString(aWriter, aParam.function());
  }

  static bool Read(MessageReader* aReader, TaintLocation* aResult) {
    std::u16string filename;
    std::u16string function;
    uint32_t line = 0, pos = 0, nextLine = 0, nextPos = 0, scriptStartLine = 0;
    TaintMd5 scriptHash{};
    if (!taint_detail::ReadStdString(aReader, &filename) ||
        !ReadParam(aReader, &line) || !ReadParam(aReader, &pos) ||
        !ReadParam(aReader, &nextLine) || !ReadParam(aReader, &nextPos) ||
        !ReadParam(aReader, &scriptStartLine) ||
        !ReadParam(aReader, &scriptHash) ||
        !taint_detail::ReadStdString(aReader, &function)) {
      return false;
    }
    *aResult = TaintLocation(std::move(filename), line, pos, nextLine, nextPos,
                             scriptStartLine, scriptHash, std::move(function));
    return true;
  }
};

// Mirrors Dump/LoadTaintOperationFromJSON: {name, location, arguments,
// isSource}.
template <>
struct ParamTraits<TaintOperation> {
  static void Write(MessageWriter* aWriter, const TaintOperation& aParam) {
    taint_detail::WriteStdString(aWriter, std::string(aParam.name()));
    WriteParam(aWriter, aParam.location());
    const std::vector<std::u16string>& args = aParam.arguments();
    WriteParam(aWriter, static_cast<uint32_t>(args.size()));
    for (const auto& arg : args) {
      taint_detail::WriteStdString(aWriter, arg);
    }
    WriteParam(aWriter, aParam.isSource());
  }

  static bool Read(MessageReader* aReader, TaintOperation* aResult) {
    std::string name;
    TaintLocation location;
    uint32_t argCount = 0;
    if (!taint_detail::ReadStdString(aReader, &name) ||
        !ReadParam(aReader, &location) || !ReadParam(aReader, &argCount)) {
      return false;
    }
    // Note: intentionally not reserve()ing argCount up front, as it arrives
    // from a potentially compromised peer; grow as elements are actually read.
    std::vector<std::u16string> args;
    for (uint32_t i = 0; i < argCount; ++i) {
      std::u16string arg;
      if (!taint_detail::ReadStdString(aReader, &arg)) {
        return false;
      }
      args.push_back(std::move(arg));
    }
    bool isSource = false;
    if (!ReadParam(aReader, &isSource)) {
      return false;
    }
    *aResult =
        TaintOperation(name.c_str(), std::move(location), std::move(args));
    if (isSource) {
      aResult->setSource();
    }
    return true;
  }
};

// Mirrors Dump/LoadTaintFlowFromJSON: an ordered list of operations from head
// (newest) to root (source). Reconstructed by extending in reverse, exactly as
// LoadTaintFlowFromJSON does.
template <>
struct ParamTraits<TaintFlow> {
  static void Write(MessageWriter* aWriter, const TaintFlow& aParam) {
    uint32_t count = 0;
    for (const auto& node : aParam) {
      (void)node;
      ++count;
    }
    WriteParam(aWriter, count);
    for (const auto& node : aParam) {
      WriteParam(aWriter, node.operation());
    }
  }

  static bool Read(MessageReader* aReader, TaintFlow* aResult) {
    uint32_t count = 0;
    if (!ReadParam(aReader, &count)) {
      return false;
    }
    // Note: intentionally not reserve()ing count up front (untrusted peer).
    std::vector<TaintOperation> ops;
    for (uint32_t i = 0; i < count; ++i) {
      TaintOperation op("");
      if (!ReadParam(aReader, &op)) {
        return false;
      }
      ops.push_back(std::move(op));
    }
    TaintFlow flow;
    for (auto it = ops.rbegin(); it != ops.rend(); ++it) {
      flow.extend(std::move(*it));
    }
    *aResult = std::move(flow);
    return true;
  }
};

// Mirrors Dump/LoadTaintRangeFromJSON: [begin, end, flow].
template <>
struct ParamTraits<TaintRange> {
  static void Write(MessageWriter* aWriter, const TaintRange& aParam) {
    WriteParam(aWriter, aParam.begin());
    WriteParam(aWriter, aParam.end());
    WriteParam(aWriter, aParam.flow());
  }

  static bool Read(MessageReader* aReader, TaintRange* aResult) {
    uint32_t begin = 0, end = 0;
    TaintFlow flow;
    if (!ReadParam(aReader, &begin) || !ReadParam(aReader, &end) ||
        !ReadParam(aReader, &flow)) {
      return false;
    }
    *aResult = TaintRange(begin, end, std::move(flow));
    return true;
  }
};

// Mirrors Dump/LoadStringTaintFromJSON: an array of ranges.
template <>
struct ParamTraits<StringTaint> {
  static void Write(MessageWriter* aWriter, const StringTaint& aParam) {
    uint32_t count = 0;
    for (const auto& range : aParam) {
      (void)range;
      ++count;
    }
    WriteParam(aWriter, count);
    for (const auto& range : aParam) {
      WriteParam(aWriter, range);
    }
  }

  static bool Read(MessageReader* aReader, StringTaint* aResult) {
    uint32_t count = 0;
    if (!ReadParam(aReader, &count)) {
      return false;
    }
    aResult->clear();
    for (uint32_t i = 0; i < count; ++i) {
      TaintRange range;
      if (!ReadParam(aReader, &range)) {
        return false;
      }
      aResult->append(std::move(range));
    }
    return true;
  }
};

}  // namespace IPC

#endif  // _TaintIPCUtils_h
