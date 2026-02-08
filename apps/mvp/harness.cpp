#include "mvp/harness.hpp"

#include <infra/app/host.hpp>
#include <infra/topology/runtime.hpp>

#include <atomic>
#include <chrono>
#include <thread>
#include <utility>

namespace magus2::mvp {

RunResult run_for(std::chrono::milliseconds duration, const MvpConfig& config, infra::app::HostHooks hooks) {
  RunResult result;

  Runtime runtime(make_topology(config), config);
  infra::app::Host host(infra::app::HostOptions {.install_signal_handlers = false, .wait_interval = std::chrono::milliseconds(1)});

  result.started = host.start(runtime, std::move(hooks));
  if (!result.started) {
    result.built = false;
    result.error = host.last_error();
    return result;
  }
  result.built = true;

  std::atomic<bool> feed_running {true};
  std::thread feeder([&]() {
    u64 seq = 1;
    auto next_emit = std::chrono::steady_clock::now();

    while (feed_running.load(std::memory_order_relaxed)) {
      TickEnvelope tick {
          .seq = seq++,
          .ts_ns = infra::topology::monotonic_ns(),
          .ctx = tlog::carrier {},
      };

      while (feed_running.load(std::memory_order_relaxed) && !runtime.try_push_tick(tick)) {
        infra::topology::relax_cpu();
      }

      next_emit += std::chrono::microseconds(config.tick_interval_us);
      const auto now = std::chrono::steady_clock::now();
      if (next_emit > now) {
        std::this_thread::sleep_until(next_emit);
      } else {
        next_emit = now;
      }
    }
  });

  std::this_thread::sleep_for(duration);
  feed_running.store(false, std::memory_order_release);
  if (feeder.joinable()) {
    feeder.join();
  }
  host.stop(runtime);

  result.stats = snapshot_from(runtime.counters());
  return result;
}

bool flow_looks_valid(const StatsSnapshot& stats) {
  if (stats.md_ticks_sent == 0U || stats.strat_ticks_seen == 0U || stats.strat_orders_sent == 0U
      || stats.or_orders_seen == 0U || stats.or_acks_sent == 0U || stats.strat_acks_seen == 0U) {
    return false;
  }

  if (stats.strat_ticks_seen > stats.md_ticks_sent) {
    return false;
  }

  if (stats.or_orders_seen > stats.strat_orders_sent) {
    return false;
  }

  if (stats.strat_acks_seen > stats.or_acks_sent) {
    return false;
  }

  const auto order_gap = stats.strat_orders_sent - stats.or_orders_seen;
  const auto ack_gap = stats.or_acks_sent - stats.strat_acks_seen;

  if (order_gap > 2U || ack_gap > 2U) {
    return false;
  }

  if (stats.trace_ticks_seen == 0U || stats.trace_acks_seen == 0U) {
    return false;
  }

  return true;
}

}  // namespace magus2::mvp
