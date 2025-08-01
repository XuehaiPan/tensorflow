/* Copyright 2022 The OpenXLA Authors.

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

#ifndef XLA_PYTHON_IFRT_EXECUTABLE_H_
#define XLA_PYTHON_IFRT_EXECUTABLE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "llvm/Support/ExtensibleRTTI.h"
#include "xla/hlo/ir/hlo_module.h"
#include "xla/pjrt/pjrt_executable.h"
#include "xla/pjrt/pjrt_layout.h"
#include "xla/python/ifrt/array.h"
#include "xla/python/ifrt/attribute_map.h"
#include "xla/python/ifrt/device.h"
#include "xla/python/ifrt/device_list.h"
#include "xla/python/ifrt/execute_options.pb.h"
#include "xla/python/ifrt/future.h"
#include "xla/python/ifrt/serdes_default_version_accessor.h"
#include "xla/python/ifrt/serdes_version.h"
#include "xla/python/ifrt/user_context.h"
#include "xla/xla_data.pb.h"

namespace xla {
namespace ifrt {

class Client;
struct CompileOptions;
struct DeserializeExecutableOptions;

// Wraps a computation that has been partially compiled and can be loaded.
class Executable : public llvm::RTTIExtends<Executable, llvm::RTTIRoot> {
 public:
  using DeserializeOptions = DeserializeExecutableOptions;

  // Unique name for this executable.
  virtual absl::string_view name() const = 0;

  // Returns a fingerprint of this executable.
  virtual absl::StatusOr<std::optional<std::string>> Fingerprint() const = 0;

  // Serializes this executable into a string. The compatibility of the
  // serialized executable is implementation-specific.
  virtual absl::StatusOr<std::string> Serialize() const = 0;

  // The following APIs are taken from `xla::PjRtExecutable` for fast
  // prototyping.
  // TODO(hyeontaek): Factor some of them out as `XlaCompatibleExecutable`.
  virtual int num_devices() const = 0;
  virtual int64_t SizeOfGeneratedCodeInBytes() const = 0;
  virtual absl::StatusOr<CompiledMemoryStats> GetCompiledMemoryStats()
      const = 0;

  // TODO(hyeontaek): Move the following XLA-specific methods to
  // pjrt_executable.h and put it in an `XlaCompatibleExecutable`.

  // Returns a list of parameter `OpSharding`.
  virtual std::optional<std::vector<OpSharding>> GetParameterShardings()
      const = 0;
  // Returns a list of output `OpSharding`.
  virtual std::optional<std::vector<OpSharding>> GetOutputShardings() const = 0;
  // Returns a list of parameter layouts.
  virtual absl::StatusOr<std::vector<std::shared_ptr<const xla::PjRtLayout>>>
  GetParameterLayouts() const = 0;
  // Returns a list of output/result layouts.
  virtual absl::StatusOr<std::vector<std::shared_ptr<const xla::PjRtLayout>>>
  GetOutputLayouts() const = 0;
  // Returns an `HloModule` (optimized) per partition.
  virtual absl::StatusOr<std::vector<std::shared_ptr<HloModule>>>
  GetHloModules() const = 0;

  // Returns a list of lists of memory kind strings for output. The returned
  // value is `[num_programs, num_output]`. The size of the outer list should be
  // equal to `GetHloModules()`. Under SPMD, one can use
  // `GetOutputMemoryKinds().front()`.
  virtual absl::StatusOr<std::vector<std::vector<absl::string_view>>>
  GetOutputMemoryKinds() const = 0;

  // Returns named values for cost properties of this executable (such as
  // operations, size of input/outputs, and run time estimate). Properties may
  // differ for different implementations and platforms.
  virtual absl::StatusOr<AttributeMap> GetCostAnalysis() const = 0;

  static char ID;  // NOLINT
};

using ExecutableRef = std::shared_ptr<Executable>;

struct ExecuteOptions {
  // If non-zero, identifies this execution as part of a potentially
  // multi-device launch. This can be used to detect scheduling errors, e.g. if
  // multi-host programs are launched in different orders on different hosts,
  // the launch IDs may be used by the runtime to detect the mismatch.
  int32_t launch_id = 0;

  // A set of indices denoting the input arrays that should not be donated. An
  // input array may be non-donable, for example, if it is referenced more than
  // once. Since such runtime information is not available at compile time, the
  // compiler might mark the input as `may-alias`, which could lead IFRT to
  // donate the input array when it should not. By defining this set of indices,
  // a higher-level IFRT caller can instruct IFRT client not to donate specific
  // input arrays.
  absl::flat_hash_set<int> non_donatable_input_indices;

  // If true, populate `ExecuteResult::status`. Otherwise, the status is left as
  // an invalid future.
  bool fill_status = false;

  // Execution stream ID identifies the series of executions that must be
  // executed in program order.  Executions with different execution stream IDs
  // may be executed in any order and concurrently.
  int64_t execution_stream_id = 0;

  // Custom execution options specific to the runtime. The user and the runtime
  // are responsible for ensuring version compatibility.
  std::optional<AttributeMap> custom_options;

  absl::StatusOr<ExecuteOptionsProto> ToProto(
      SerDesVersion version = SerDesDefaultVersionAccessor::Get()) const;

  static absl::StatusOr<ExecuteOptions> FromProto(
      const ExecuteOptionsProto& proto);
};

// Wraps a computation that has been fully compiled and loaded for execution.
class LoadedExecutable
    : public llvm::RTTIExtends<LoadedExecutable, llvm::RTTIRoot> {
 public:
  virtual Client* client() const = 0;

  // Executable methods. Note that LoadedExecutable does not inherit from
  // Executable to avoid multiple inheritance in LoadedExecutable
  // implementations.

  // Unique name for this executable.
  virtual absl::string_view name() const = 0;

  // Returns a fingerprint of this executable.
  virtual absl::StatusOr<std::optional<std::string>> Fingerprint() const = 0;

  // Serializes this executable into a string. The compatibility of the
  // serialized executable is implementation-specific.
  virtual absl::StatusOr<std::string> Serialize() const = 0;

  // Returns the user context associated with the creation of this executable.
  // May be `nullptr` if the user context is unset or the runtime does not
  // support it.
  virtual UserContextRef user_context() const = 0;

  // Returns a future that becomes ready when the executable is ready to be
  // used for execution.
  //
  // This can be used by implementations that support async compilation, where
  // `Compiler::Compile()` returns an executable ~immediately and does heavy
  // compilation work in the background. Implementations must still ensure that
  // all other methods can be used even without explicitly waiting for the ready
  // future (e.g., via blocking).
  virtual Future<> GetReadyFuture() const = 0;

  // The following APIs are taken from `xla::PjRtExecutable` for fast
  // prototyping.

  // TODO(hyeontaek): Factor some of them out as `XlaCompatibleExecutable`.
  virtual int num_devices() const = 0;
  virtual int64_t SizeOfGeneratedCodeInBytes() const = 0;
  virtual absl::StatusOr<CompiledMemoryStats> GetCompiledMemoryStats()
      const = 0;

  // The following APIs are taken from `xla::PjRtLoadedExecutable` for fast
  // prototyping.

  // TODO(hyeontaek): Move the following to pjrt_executable.h and put it in an
  // `XlaCompatibleExecutable`.
  // Returns a list of parameter Sharding.
  virtual std::optional<std::vector<OpSharding>> GetParameterShardings()
      const = 0;

  // Returns the indices of parameters that will be donated whenever `Execute`
  // gets called, provided they are not present in
  // `execute_options.non_donatable_input_indices`.
  virtual absl::StatusOr<absl::Span<const int>> GetDonatableInputIndices()
      const = 0;

  // Returns a list of output OpSharding.
  virtual std::optional<std::vector<OpSharding>> GetOutputShardings() const = 0;
  // Returns a list of parameter layouts.
  virtual absl::StatusOr<std::vector<std::shared_ptr<const xla::PjRtLayout>>>
  GetParameterLayouts() const = 0;
  // Returns a list of output/result layouts.
  virtual absl::StatusOr<std::vector<std::shared_ptr<const xla::PjRtLayout>>>
  GetOutputLayouts() const = 0;
  // Return an HloModule (optimized) per partition.
  virtual absl::StatusOr<std::vector<std::shared_ptr<HloModule>>>
  GetHloModules() const = 0;
  // Returns a list of lists of memory kind strings for output. The returned
  // value is `[num_programs, num_output]`. The size of the outer list should be
  // equal to `GetHloModules()`. Under SPMD, one can use
  // `GetOutputMemoryKinds().front()`.
  virtual absl::StatusOr<std::vector<std::vector<absl::string_view>>>
  GetOutputMemoryKinds() const = 0;

  // Returns named values for cost properties of this executable (such as
  // operations, size of input/outputs, and run time estimate). Properties may
  // differ for different implementations and platforms.
  virtual absl::StatusOr<AttributeMap> GetCostAnalysis() const = 0;

  // `LoadedExecutable` methods.

  using ExecuteOptions = ::xla::ifrt::ExecuteOptions;

  // Result from an execution.
  struct ExecuteResult {
    // Resulting status of the execution. Filled only if
    // `ExecuteOptions::fill_status` is true.
    Future<> status;
    // Output arrays.
    std::vector<ArrayRef> outputs;
  };

  // Executes the executable on devices.
  //
  // The runtime expects input arrays to be present on the execution devices.
  //
  // If `devices` is specified, the execution runs on the devices if the runtime
  // supports. Otherwise, the execution runs on the devices where the executable
  // has been compiled and loaded onto.
  //
  // TODO(hyeontaek): This call does not have strict "barrier" semantics, and
  // thus it is up to the backend implementation: Some backends will wait all
  // arguments to be available to run any computation (which may be composed of
  // individually dispatchable sub-computations), while others may run the
  // computation incrementally. Some backends will mark outputs to become ready
  // roughly at the same time, while others may make outputs ready
  // incrementally. We need to have a stricter way to control this behavior
  // (e.g., having per-argument/output booleans or providing a separate barrier
  // API).
  virtual absl::StatusOr<ExecuteResult> Execute(
      absl::Span<ArrayRef> args, const ExecuteOptions& options,
      std::optional<DeviceListRef> devices) = 0;

  // The following APIs are taken from xla::PjRtLoadedExecutable for fast
  // prototyping.
  // TODO(hyeontaek): Move the following XLA-specific methods to
  // pjrt_executable.h and put it in an `XlaCompatibleExecutable`.

  virtual absl::Span<Device* const> addressable_devices() const = 0;

  static char ID;  // NOLINT
};

using LoadedExecutableRef = std::shared_ptr<LoadedExecutable>;

}  // namespace ifrt
}  // namespace xla

#endif  // XLA_PYTHON_IFRT_EXECUTABLE_H_
