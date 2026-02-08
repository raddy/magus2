#include "infra/app/host.hpp"

#include <thread>

namespace magus2::infra::app {
namespace {

std::atomic<bool>* g_stop_requested = nullptr;

void signal_stop_handler(int /*sig*/) {
  if (g_stop_requested != nullptr) {
    g_stop_requested->store(true, std::memory_order_release);
  }
}

}  // namespace

Host::Host(HostOptions options)
    : options_(options) {}

Host::~Host() {
  uninstall_signal_handlers();
  run_teardown();
}

void Host::request_stop() noexcept {
  stop_requested_.store(true, std::memory_order_release);
}

bool Host::stop_requested() const noexcept {
  return stop_requested_.load(std::memory_order_acquire);
}

void Host::wait_for_stop() {
  while (!stop_requested()) {
    std::this_thread::sleep_for(options_.wait_interval);
  }
}

void Host::wait_for(std::chrono::milliseconds duration) {
  const auto deadline = std::chrono::steady_clock::now() + duration;
  while (!stop_requested() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(options_.wait_interval);
  }
}

const std::string& Host::last_error() const noexcept {
  return error_;
}

bool Host::install_signal_handlers() {
  if (!options_.install_signal_handlers) {
    return true;
  }
  if (signals_installed_) {
    return true;
  }
  if (g_stop_requested != nullptr) {
    error_ = "signal handlers already owned by another host";
    return false;
  }

  g_stop_requested = &stop_requested_;
  prev_sigint_ = std::signal(SIGINT, signal_stop_handler);
  if (prev_sigint_ == SIG_ERR) {
    g_stop_requested = nullptr;
    error_ = "failed to install SIGINT handler";
    return false;
  }

  prev_sigterm_ = std::signal(SIGTERM, signal_stop_handler);
  if (prev_sigterm_ == SIG_ERR) {
    std::signal(SIGINT, prev_sigint_);
    g_stop_requested = nullptr;
    error_ = "failed to install SIGTERM handler";
    return false;
  }

  signals_installed_ = true;
  return true;
}

void Host::uninstall_signal_handlers() noexcept {
  if (!signals_installed_) {
    return;
  }

  (void)std::signal(SIGINT, prev_sigint_);
  (void)std::signal(SIGTERM, prev_sigterm_);
  g_stop_requested = nullptr;
  signals_installed_ = false;
}

void Host::run_teardown() noexcept {
  if (!setup_done_ || !teardown_) {
    teardown_ = {};
    setup_done_ = false;
    return;
  }

  teardown_();
  teardown_ = {};
  setup_done_ = false;
}

}  // namespace magus2::infra::app
