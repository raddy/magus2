#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace magus2::infra::topology {

struct WorkerSpec {
  std::string_view name;
  std::uint32_t core;
  std::function<void()> run;
};

class ThreadRuntime {
public:
  ThreadRuntime() = default;
  ~ThreadRuntime();

  ThreadRuntime(const ThreadRuntime&) = delete;
  ThreadRuntime& operator=(const ThreadRuntime&) = delete;

  [[nodiscard]] bool launch(const std::vector<WorkerSpec>& workers);
  void join();

  [[nodiscard]] const std::string& last_error() const noexcept;

  static bool pin_current_thread(std::uint32_t core) noexcept;

private:
  std::vector<std::thread> threads_;
  std::string error_;
};

void relax_cpu() noexcept;
[[nodiscard]] std::uint64_t monotonic_ns() noexcept;

}  // namespace magus2::infra::topology
