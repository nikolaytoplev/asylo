/*
 *
 * Copyright 2017 Asylo authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef ASYLO_IDENTITY_IDENTITY_EXPECTATION_MATCHER_H_
#define ASYLO_IDENTITY_IDENTITY_EXPECTATION_MATCHER_H_

#include "asylo/identity/identity.pb.h"
#include "asylo/util/statusor.h"

namespace asylo {

/// Defines an abstract interface that describes how to match an
/// `EnclaveIdentity` against an `EnclaveIdentityExpectation`.
///
/// All implementations of this interface are expected to be thread-safe.
class IdentityExpectationMatcher {
 public:
  IdentityExpectationMatcher() = default;
  virtual ~IdentityExpectationMatcher() = default;

  /// Evaluates whether `identity` matches `expectation`.
  ///
  /// Evaluating `identity` against `expectation` produces a boolean result
  /// indicating whether `identity` matches `expectation`, but only if the
  /// inputs are valid for this matcher. Otherwise, if `matcher` does not
  /// understand either `identity` or `expectation`, this method returns a
  /// non-OK Status. This can happen if any of the following is true:
  ///
  ///  * `identity.description()` is unrecognized by the matcher
  ///  * `expectation.reference_identity().description()` is unrecognized by the
  ///  matcher
  ///  * `identity` and/or `expectation` is malformed
  ///
  /// An IdentityExpectationMatcher's Match() implementation is not obliged to
  /// handle all possible `EnclaveIdentity` and `EnclaveIdentityExpectation`
  /// protos. Rather, each implementation of IdentityExpectationMatcher is free
  /// to refine expectations on what kinds of `EnclaveIdentity` and
  /// `EnclaveIdentityExpectation` arguments it can handle. It is up to the
  /// caller of this method to provide inputs that fit the expectations of the
  /// underlying matcher implementation.
  ///
  /// \param identity An identity to match.
  /// \param expectation The identity expectation to match against.
  /// \return A bool indicating whether the match succeeded, or a non-OK Status
  ///         in the case of invalid arguments.
  virtual StatusOr<bool> Match(
      const EnclaveIdentity &identity,
      const EnclaveIdentityExpectation &expectation) const = 0;
};

}  // namespace asylo

#endif  // ASYLO_IDENTITY_IDENTITY_EXPECTATION_MATCHER_H_
