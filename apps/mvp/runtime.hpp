#pragma once

#include "mvp/topology.hpp"
#include "mvp/stats.hpp"

#include <memory>
#include <string>

namespace magus2::mvp {

class Runtime {
public:
  Runtime(infra::topology::Topology topology, MvpConfig config);
  ~Runtime();

  bool build();
  bool start();
  void stop();
  void join();

  bool try_push_tick(const TickEnvelope& tick) noexcept;

  [[nodiscard]] const RuntimeCounters& counters() const noexcept;
  [[nodiscard]] const std::string& last_error() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace magus2::mvp
