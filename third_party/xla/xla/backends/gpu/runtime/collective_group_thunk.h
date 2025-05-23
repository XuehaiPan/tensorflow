/* Copyright 2024 The OpenXLA Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef XLA_BACKENDS_GPU_RUNTIME_COLLECTIVE_GROUP_THUNK_H_
#define XLA_BACKENDS_GPU_RUNTIME_COLLECTIVE_GROUP_THUNK_H_

#include <memory>
#include <vector>

#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "xla/backends/gpu/collectives/gpu_clique_key.h"
#include "xla/backends/gpu/runtime/collective_thunk.h"
#include "xla/backends/gpu/runtime/thunk.h"
#include "xla/hlo/ir/hlo_instruction.h"

namespace xla {
namespace gpu {

// Collective group thunk fuses together a set of arbitrary collective
// operations into a single group call in order for them to be dispatched
// together. Implementation is backend-specific and might not be supported by
// all collective implementations.

class CollectiveGroupThunk : public Thunk {
 public:
  CollectiveGroupThunk(const HloInstruction* instruction, Thunk::Kind kind,
                       std::vector<std::unique_ptr<Thunk>> thunks,
                       AsyncStreamKind stream_kind);
  absl::Status Prepare(const PrepareParams& params,
                       ResourceRequestsInterface& resource_requests) override;
  absl::Status ExecuteOnStream(const Thunk::ExecuteParams& params) override;
  absl::Status Initialize(const InitializeParams& params) override;
  void ForAllThunks(absl::FunctionRef<void(const Thunk*)> fn) const override;
  std::shared_ptr<CollectiveThunk::AsyncEvents> async_events() const {
    return async_events_;
  }

 private:
  ThunkSequence thunks_;
  AsyncStreamKind stream_kind_;
  std::shared_ptr<CollectiveThunk::AsyncEvents> async_events_;
};

}  // namespace gpu
}  // namespace xla

#endif  // XLA_BACKENDS_GPU_RUNTIME_COLLECTIVE_GROUP_THUNK_H_
