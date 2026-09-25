/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Modifications Copyright SAP SE. 2019-2021.  All rights reserved.
 */

#ifndef nsTaintingUtils_h__
#define nsTaintingUtils_h__

#include "mozilla/Maybe.h"
#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/Element.h"
#include "nsINode.h"
#include "nsString.h"

#include "jsapi.h"


// Get a taint operation
TaintOperation GetTaintOperation(const char* name);

TaintLocation GetTaintLocation();

// Extend the taintflow
nsresult MarkTaintOperation(nsAString &str, const char* name);
nsresult MarkTaintOperation(nsAString &str, const char* name, const nsINode* node);
nsresult MarkTaintOperation(nsACString &str, const char* name);
nsresult MarkTaintOperation(nsACString &str, const char* name, const nsACString &arg);
nsresult MarkTaintOperation(nsAString &str, const char* name, const nsTArray<nsString> &arg);
nsresult MarkTaintOperation(nsACString &str, const char* name, const nsTArray<nsString> &arg);
nsresult MarkTaintOperation(nsCString &str, const char* name, const nsTArray<nsCString> &arg);
nsresult MarkTaintOperation(StringTaint& aTaint, const char* name);

// Foxhound: Record a write to the DOM in the flow of the value written, with the
// element written to and one further argument: the attribute's local name for an
// attribute write, the position for insertAdjacentHTML.
//
// A consumer that later sees the value read back off the live document needs to
// know how it got there. An attribute written through the attribute API is stored
// and returned verbatim; one parsed out of markup is entity decoded on the way
// back out. The two produce different results from the same recorded string.
nsresult MarkTaintOperation(nsAString &str, const char* name, const nsINode* node,
                            const nsAString &arg);

// Foxhound: Record a scripted markup write in the flow of the value written, and
// return the string to parse: a copy carrying the operation when the value is
// tainted, and the value itself when it is not.
//
// This is the other half of a DOM round-trip. A flow that writes markup into the
// document and reads it back is a desanitization mechanism in its own right --
// what comes back out is the browser's re-serialization of what went in -- and a
// consumer can only reproduce that if the write is in the flow too. `position` is
// the insertAdjacentHTML position, and null for the writes that have none.
//
// The characters are copied before the operation is added because a caller's
// string can share its buffer with the one handed to us and taint lives on the
// buffer, so extending in place would retroactively add this write to the flow of
// a string the caller still holds.
const nsAString& TaintMarkupWrite(const nsAString& value, const char* name,
                                  const nsINode* node,
                                  mozilla::Maybe<nsAutoString>& holder,
                                  const nsAString* position = nullptr);

// Foxhound: Add taint source information to a string
nsresult MarkTaintSource(nsAString &str, const char* name);
nsresult MarkTaintSource(nsACString &str, const char* name);

// Foxhound: Add taint source information to a string
nsresult MarkTaintSource(nsAString &str, const char* name, const nsAString &arg);

nsresult MarkTaintSource(nsAString &str, const char* name, const nsTArray<nsString> &arg);

nsresult MarkTaintSourceElement(nsAString &str, const char* name, const nsINode* node);

// Foxhound: Add taint source information to a string
nsresult MarkTaintSource(mozilla::dom::DOMString &str, const char* name);

// Foxhound: Add taint source information to a string
nsresult MarkTaintSource(mozilla::dom::DOMString &str, const char* name, const nsAString &arg);

nsresult MarkTaintSource(mozilla::dom::DOMString &str, const char* name, const nsTArray<nsString> &arg);

nsresult MarkTaintSourceElement(mozilla::dom::DOMString &str, const char* name, const nsINode* node);

// Foxhound: Add taint source information to a string
nsresult MarkTaintSourceAttribute(nsAString &str, const char* name, const mozilla::dom::Element* node,
                                  const nsAString &attr);

nsresult MarkTaintSourceAttribute(mozilla::dom::DOMString &str, const char* name, const mozilla::dom::Element* node,
                                  const nsAString &attr);

nsresult MarkTaintSource(JSContext* aCx, JS::MutableHandle<JS::Value> aValue, const char* name);

nsresult MarkTaintSource(JSContext* aCx, JS::MutableHandle<JS::Value> aValue, const char* name, const nsAString &arg);

nsresult MarkTaintSource(JSContext* aCx, JSString* str, const char* name);

nsresult MarkTaintSource(JSContext* aCx, JSString* str, const char* name, const nsAString &arg);

nsresult MarkTaintSource(TaintFlow &flow, const char* name, const nsAString &arg);

// Foxhound: Add taint source information for a selector-based element lookup.
// `arg` is the selector the lookup used and `node` the element it returned, which
// is recorded as an XPath so a consumer can find that element again without
// re-running the query against the live document. `aMatchCount` and `aMatchIndex`
// place the match inside a multi-element result and are left out of the operation
// when `aMatchCount` is negative.
//
// The XPath, count and index are recorded only under --enable-taint-selector-xpath,
// because describing an element costs a walk up its ancestors that also counts each
// ancestor's preceding siblings, and this runs once per *matched* element. A lookup
// returning many matches out of a long list therefore goes superlinear: measured at
// 16x on getElementsByClassName over 20 matches under a 2000-child parent. Without
// the flag only the selector is recorded, exactly as before the argument existed.
nsresult MarkTaintSourceSelector(TaintFlow &flow, const char* name, const nsAString &arg,
                                 const nsINode* node, int32_t aMatchCount,
                                 int32_t aMatchIndex);

// Foxhound: Report taint flows into DOM related sinks.
nsresult ReportTaintSink(JSContext *cx, const nsAString &str, const char* name);

// Foxhound: Report taint flows into DOM related sinks.
nsresult ReportTaintSink(const nsAString &str, const char* name);

nsresult ReportTaintSink(const nsAString &str, const char* name, const nsINode* node);

// Foxhound: Report a sink that is an attribute of `node`, recording the XPath as
// arguments[0] and the attribute's local name as arguments[1]. The name is needed
// where one sink covers a family of attributes, as the event handler sink does.
nsresult ReportTaintSink(const nsAString &str, const char* name, const nsINode* node,
                         const nsAString &attr);

nsresult ReportTaintSink(const nsACString &str, const char* name);

nsresult ReportTaintSink(JSContext *cx, const nsAString &str, const char* name, const nsAString &arg);

nsresult ReportTaintSink(JSContext *cx, const nsACString &str, const char* name, const nsAString &arg);

nsresult ReportTaintSink(const nsAString &str, const char* name, const nsAString &arg);

nsresult ReportTaintSink(const nsACString &str, const char* name, const nsAString &arg);

nsresult ReportTaintSink(JSContext* cx, JS::Handle<JS::Value> aValue, const char* name);

nsresult ReportTaintSink(JSContext* cx, JS::Handle<JS::Value> aValue, const char* name, const nsAString &arg);

#endif /* nsTaintingUtils_h__ */
