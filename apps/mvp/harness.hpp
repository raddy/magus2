#pragma once

#include "mvp/runtime.hpp"
#include "mvp/stats.hpp"

#include <infra/app/host.hpp>

#include <chrono>
#include <string>

namespace magus2::mvp {

struct RunResult {
  bool built {false};
  bool started {false};
  StatsSnapshot stats {};
  std::string error;
};

RunResult run_for(
    std::chrono::milliseconds duration,
    const MvpConfig& config = {},
    infra::app::HostHooks hooks = {});
[[nodiscard]] bool flow_looks_valid(const StatsSnapshot& stats);

}  // namespace magus2::mvp
