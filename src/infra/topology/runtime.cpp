#include "infra/topology/runtime.hpp"

#include <chrono>

#if defined(__x86_64__) || defined(_M_X64)
#  include <immintrin.h>
#endif

#if defined(__linux__)
#  include <pthread.h>
#  include <sched.h>
#endif

namespace magus2::infra::topology {

ThreadRuntime::~ThreadRuntime() {
  join();
}

bool ThreadRuntime::launch(const std::vector<WorkerSpec>& workers) {
  if (!threads_.empty()) {
    error_ = "threads already running";
    return false;
  }

  threads_.reserve(workers.size());
  for (const WorkerSpec& worker : workers) {
    if (!worker.run) {
      error_ = "worker run function is empty";
      join();
      return false;
    }

    threads_.emplace_back([worker]() {
      (void)pin_current_thread(worker.core);
      worker.run();
    });
  }

  return true;
}

void ThreadRuntime::join() {
  for (std::thread& thread : threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  threads_.clear();
}

const std::string& ThreadRuntime::last_error() const noexcept {
  return error_;
}

bool ThreadRuntime::pin_current_thread(std::uint32_t core) noexcept {
#if defined(__linux__)
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(static_cast<int>(core), &set);
  return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set) == 0;
#else
  (void)core;
  return true;
#endif
}

void relax_cpu() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
  _mm_pause();
#else
  std::this_thread::yield();
#endif
}

std::uint64_t monotonic_ns() noexcept {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

}  // namespace magus2::infra::topology
