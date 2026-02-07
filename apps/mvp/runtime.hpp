#pragma once

#include "mvp/topology.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace magus2::mvp {

struct StatsSnapshot {
  std::uint64_t md_ticks_sent {0};
  std::uint64_t strat_ticks_seen {0};
  std::uint64_t strat_orders_sent {0};
  std::uint64_t strat_acks_seen {0};
  std::uint64_t or_orders_seen {0};
  std::uint64_t or_acks_sent {0};

  std::uint64_t tick_one_way_count {0};
  std::uint64_t tick_one_way_sum_ns {0};
  std::uint64_t tick_one_way_max_ns {0};

  std::uint64_t order_rtt_count {0};
  std::uint64_t order_rtt_sum_ns {0};
  std::uint64_t order_rtt_max_ns {0};

  std::uint64_t trace_ticks_seen {0};
  std::uint64_t trace_acks_seen {0};

  [[nodiscard]] double tick_one_way_avg_ns() const noexcept {
    if (tick_one_way_count == 0U) {
      return 0.0;
    }
    return static_cast<double>(tick_one_way_sum_ns) / static_cast<double>(tick_one_way_count);
  }

  [[nodiscard]] double order_rtt_avg_ns() const noexcept {
    if (order_rtt_count == 0U) {
      return 0.0;
    }
    return static_cast<double>(order_rtt_sum_ns) / static_cast<double>(order_rtt_count);
  }
};

struct RunResult {
  bool built {false};
  bool started {false};
  StatsSnapshot stats {};
  std::string error;
};

class Runtime {
public:
  Runtime(infra::topology::Topology topology, MvpConfig config);
  ~Runtime();

  bool build();
  bool start();
  void stop();
  void join();

  [[nodiscard]] StatsSnapshot snapshot() const;
  [[nodiscard]] const std::string& last_error() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

RunResult run_for(std::chrono::milliseconds duration, const MvpConfig& config = {});
[[nodiscard]] bool flow_looks_valid(const StatsSnapshot& stats);

}  // namespace magus2::mvp
