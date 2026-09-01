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

// Foxhound: Record a scripted attribute write in the flow of the value written.
// `node` is the element written to and `attr` the attribute's local name, so a
// consumer that later sees the value read back off the live document knows it
// was put there through the attribute API -- where it is stored and returned
// verbatim -- rather than parsed out of markup, where reading it back entity
// decodes it. The two produce different results from the same recorded string.
nsresult MarkTaintOperationAttribute(nsAString &str, const char* name, const nsINode* node,
                                     const nsAString &attr);

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
