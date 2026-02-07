#pragma once

#include "mvp/topology.hpp"

#include <chrono>
#include <memory>
#include <string>

namespace magus2::mvp {

struct StatsSnapshot {
  infra::u64 md_ticks_sent {0};
  infra::u64 strat_ticks_seen {0};
  infra::u64 strat_orders_sent {0};
  infra::u64 strat_acks_seen {0};
  infra::u64 or_orders_seen {0};
  infra::u64 or_acks_sent {0};

  infra::u64 tick_one_way_count {0};
  infra::u64 tick_one_way_sum_ns {0};
  infra::u64 tick_one_way_max_ns {0};

  infra::u64 order_rtt_count {0};
  infra::u64 order_rtt_sum_ns {0};
  infra::u64 order_rtt_max_ns {0};

  infra::u64 trace_ticks_seen {0};
  infra::u64 trace_acks_seen {0};

  [[nodiscard]] double tick_one_way_avg_ns() const noexcept;
  [[nodiscard]] double order_rtt_avg_ns() const noexcept;
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

  bool try_push_tick(const Tick& tick) noexcept;

  [[nodiscard]] StatsSnapshot snapshot() const;
  [[nodiscard]] const std::string& last_error() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

RunResult run_for(std::chrono::milliseconds duration, const MvpConfig& config = {});
[[nodiscard]] bool flow_looks_valid(const StatsSnapshot& stats);

}  // namespace magus2::mvp
