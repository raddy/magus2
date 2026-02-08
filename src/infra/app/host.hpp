#pragma once

#include <atomic>
#include <chrono>
#include <csignal>
#include <functional>
#include <string>
#include <utility>

namespace magus2::infra::app {

struct HostOptions {
  bool install_signal_handlers {true};
  std::chrono::milliseconds wait_interval {20};
};

struct HostHooks {
  std::function<void()> setup;
  std::function<void()> teardown;
};

class Host {
public:
  explicit Host(HostOptions options = {});
  ~Host();

  Host(const Host&) = delete;
  Host& operator=(const Host&) = delete;

  template<typename RuntimeT>
  bool start(RuntimeT& runtime, HostHooks hooks = {}) {
    if (started_) {
      return true;
    }

    error_.clear();
    stop_requested_.store(false, std::memory_order_release);

    if (hooks.setup) {
      hooks.setup();
      setup_done_ = true;
    } else {
      setup_done_ = false;
    }
    teardown_ = std::move(hooks.teardown);

    if (!runtime.build()) {
      error_ = runtime.last_error();
      run_teardown();
      return false;
    }

    if (!runtime.start()) {
      error_ = runtime.last_error();
      run_teardown();
      return false;
    }

    if (!install_signal_handlers()) {
      runtime.stop();
      runtime.join();
      run_teardown();
      return false;
    }

    started_ = true;
    return true;
  }

  template<typename RuntimeT>
  void stop(RuntimeT& runtime) noexcept {
    request_stop();
    runtime.stop();
    runtime.join();
    uninstall_signal_handlers();
    run_teardown();
    started_ = false;
  }

  void request_stop() noexcept;
  [[nodiscard]] bool stop_requested() const noexcept;

  void wait_for_stop();
  void wait_for(std::chrono::milliseconds duration);

  [[nodiscard]] const std::string& last_error() const noexcept;

private:
  using SignalHandler = void (*)(int);

  bool install_signal_handlers();
  void uninstall_signal_handlers() noexcept;
  void run_teardown() noexcept;

  HostOptions options_;
  std::atomic<bool> stop_requested_ {false};
  std::string error_;
  bool started_ {false};
  bool setup_done_ {false};
  std::function<void()> teardown_;
  SignalHandler prev_sigint_ {SIG_DFL};
  SignalHandler prev_sigterm_ {SIG_DFL};
  bool signals_installed_ {false};
};

}  // namespace magus2::infra::app
