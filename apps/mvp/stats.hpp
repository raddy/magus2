#pragma once

#include <infra/types/short_types.hpp>

#include <atomic>

namespace magus2::mvp {

struct RuntimeCounters {
  std::atomic<infra::u64> md_ticks_sent {0};
  std::atomic<infra::u64> strat_ticks_seen {0};
  std::atomic<infra::u64> strat_orders_sent {0};
  std::atomic<infra::u64> strat_acks_seen {0};
  std::atomic<infra::u64> or_orders_seen {0};
  std::atomic<infra::u64> or_acks_sent {0};

  std::atomic<infra::u64> tick_one_way_count {0};
  std::atomic<infra::u64> tick_one_way_sum_ns {0};
  std::atomic<infra::u64> tick_one_way_max_ns {0};

  std::atomic<infra::u64> order_rtt_count {0};
  std::atomic<infra::u64> order_rtt_sum_ns {0};
  std::atomic<infra::u64> order_rtt_max_ns {0};

  std::atomic<infra::u64> trace_ticks_seen {0};
  std::atomic<infra::u64> trace_acks_seen {0};
};

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

[[nodiscard]] inline StatsSnapshot snapshot_from(const RuntimeCounters& counters) {
  return StatsSnapshot {
      .md_ticks_sent = counters.md_ticks_sent.load(std::memory_order_relaxed),
      .strat_ticks_seen = counters.strat_ticks_seen.load(std::memory_order_relaxed),
      .strat_orders_sent = counters.strat_orders_sent.load(std::memory_order_relaxed),
      .strat_acks_seen = counters.strat_acks_seen.load(std::memory_order_relaxed),
      .or_orders_seen = counters.or_orders_seen.load(std::memory_order_relaxed),
      .or_acks_sent = counters.or_acks_sent.load(std::memory_order_relaxed),
      .tick_one_way_count = counters.tick_one_way_count.load(std::memory_order_relaxed),
      .tick_one_way_sum_ns = counters.tick_one_way_sum_ns.load(std::memory_order_relaxed),
      .tick_one_way_max_ns = counters.tick_one_way_max_ns.load(std::memory_order_relaxed),
      .order_rtt_count = counters.order_rtt_count.load(std::memory_order_relaxed),
      .order_rtt_sum_ns = counters.order_rtt_sum_ns.load(std::memory_order_relaxed),
      .order_rtt_max_ns = counters.order_rtt_max_ns.load(std::memory_order_relaxed),
      .trace_ticks_seen = counters.trace_ticks_seen.load(std::memory_order_relaxed),
      .trace_acks_seen = counters.trace_acks_seen.load(std::memory_order_relaxed),
  };
}

}  // namespace magus2::mvp
