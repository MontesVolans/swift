//===--- OptimizationRemark.h - Optimization diagnostics --------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This file defines the remark type and the emitter class that passes can use
/// to emit optimization diagnostics.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_SIL_OPTIMIZATIONREMARKEMITTER_H
#define SWIFT_SIL_OPTIMIZATIONREMARKEMITTER_H

#include "swift/Basic/SourceLoc.h"
#include "swift/SIL/SILInstruction.h"
#include "llvm/ADT/StringRef.h"

namespace swift {

class ASTContext;
class SILFunction;

namespace OptRemark {

/// \brief Used in the streaming interface as the general argument type.  It
/// internally converts everything into a key-value pair.
struct Argument {
  std::string Key;
  std::string Val;
  /// If set, the debug location corresponding to the value.
  SourceLoc Loc;

  explicit Argument(StringRef Str = "") : Key("String"), Val(Str) {}

  Argument(StringRef Key, int N);
  Argument(StringRef Key, long N);
  Argument(StringRef Key, long long N);
  Argument(StringRef Key, unsigned N);
  Argument(StringRef Key, unsigned long N);
  Argument(StringRef Key, unsigned long long N);

  Argument(StringRef Key, SILFunction *F);
};

/// Shorthand to insert named-value pairs.
using NV = Argument;

/// The base class for remarks.  This can be created by optimization passed to
/// report successful and unsuccessful optimizations. CRTP is used to preserve
/// the underlying type encoding the remark kind in the insertion operator.
template <typename DerivedT> class Remark {
  /// Arguments collected via the streaming interface.
  SmallVector<Argument, 4> Args;

  /// Textual identifier for the remark (single-word, camel-case). Can be used
  /// by external tools reading the YAML output file for optimization remarks to
  /// identify the remark.
  StringRef Identifier;

  /// Source location for the diagnostics.
  SourceLoc Location;

protected:
  Remark(StringRef Identifier, SILInstruction &I)
      : Identifier(Identifier), Location(I.getLoc().getSourceLoc()) {}

public:
  DerivedT &operator<<(StringRef S) {
    Args.emplace_back(S);
    return *static_cast<DerivedT *>(this);
  }

  DerivedT &operator<<(Argument A) {
    Args.push_back(std::move(A));
    return *static_cast<DerivedT *>(this);
  }

  SourceLoc getLocation() const { return Location; }
  std::string getMsg() const;
};

/// Remark to report a successful optimization.
struct RemarkPassed : public Remark<RemarkPassed> {
  RemarkPassed(StringRef Id, SILInstruction &I) : Remark(Id, I) {}
};
/// Remark to report a unsuccessful optimization.
struct RemarkMissed : public Remark<RemarkMissed> {
  RemarkMissed(StringRef Id, SILInstruction &I) : Remark(Id, I) {}
};

/// Used to emit the remarks.  Passes reporting remarks should create an
/// instance of this.
class Emitter {
  ASTContext &Ctx;
  std::string PassName;
  bool PassedEnabled;
  bool MissedEnabled;

  void emit(const RemarkPassed &R);
  void emit(const RemarkMissed &R);

  template <typename RemarkT> bool isEnabled();

public:
  Emitter(StringRef PassName, ASTContext &Ctx);

  /// \brief Take a lambda that returns a remark which will be emitted.  The
  /// lambda is not evaluated unless remarks are enabled.  Second argument is
  /// only used to restrict this to functions.
  template <typename T>
  void emit(T RemarkBuilder, decltype(RemarkBuilder()) * = nullptr) {
    using RemarkT = decltype(RemarkBuilder());
    // Avoid building the remark unless remarks are enabled.
    if (isEnabled<RemarkT>()) {
      auto R = RemarkBuilder();
      emit(R);
    }
  }
};

template <> inline bool Emitter::isEnabled<RemarkMissed>() {
  return MissedEnabled;
}
template <> inline bool Emitter::isEnabled<RemarkPassed>() {
  return PassedEnabled;
}
} // namespace OptRemark
} // namespace swift
#endif
