#include "mvp/runtime.hpp"

#include "mvp/nodes.hpp"
#include "mvp/wiring.hpp"

#include <infra/topology/runtime.hpp>
#include <infra/topology/spec.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace magus2::mvp {

struct Runtime::Impl {
  explicit Impl(infra::topology::Topology topology, MvpConfig config)
      : topology(std::move(topology))
      , config(config) {}

  infra::topology::Topology topology;
  MvpConfig config;

  wiring::QueueStore queue_store;
  infra::topology::ThreadRuntime thread_runtime;

  RuntimeStats stats;
  std::atomic<bool> running {false};

  infra::topology::TxPort<Tick> driver_tick_tx;

  std::optional<MdNode> md;
  std::optional<StratNode> strat;
  std::optional<OrNode> orr;

  bool built {false};
  bool started {false};
  std::string error;

  [[nodiscard]] bool build() {
    if (built) {
      return true;
    }

    if (!infra::topology::validate_ports(topology, error)) {
      return false;
    }

    if (!queue_store.build(topology, error)) {
      return false;
    }

    MdPorts md_ports;
    StratPorts strat_ports;
    OrPorts or_ports;

    if (!wiring::bind_tx_port(driver_tick_tx, queue_store, topology, NodeId::Driver, "tick_tx", Contract::Tick, error)) {
      return false;
    }

    if (!wiring::bind_rx_port(md_ports.tick_rx, queue_store, topology, NodeId::Md, "tick_rx", Contract::Tick, error)
        || !wiring::bind_tx_port(md_ports.tick_tx, queue_store, topology, NodeId::Md, "tick_tx", Contract::Tick, error)) {
      return false;
    }

    if (!wiring::bind_rx_port(strat_ports.tick_rx, queue_store, topology, NodeId::Strat, "tick_rx", Contract::Tick, error)
        || !wiring::bind_tx_port(
            strat_ports.order_tx, queue_store, topology, NodeId::Strat, "order_tx", Contract::OrderReq, error)
        || !wiring::bind_rx_port(strat_ports.ack_rx, queue_store, topology, NodeId::Strat, "ack_rx", Contract::OrderAck, error)) {
      return false;
    }

    if (!wiring::bind_rx_port(or_ports.order_rx, queue_store, topology, NodeId::Or, "order_rx", Contract::OrderReq, error)
        || !wiring::bind_tx_port(or_ports.ack_tx, queue_store, topology, NodeId::Or, "ack_tx", Contract::OrderAck, error)) {
      return false;
    }

    md.emplace(md_ports, running, stats, wiring::trace_thread_idx(NodeId::Md));
    strat.emplace(strat_ports, running, stats, config.order_every_n_ticks, wiring::trace_thread_idx(NodeId::Strat));
    orr.emplace(or_ports, running, stats, wiring::trace_thread_idx(NodeId::Or));

    built = true;
    return true;
  }

  [[nodiscard]] bool start() {
    if (!built && !build()) {
      return false;
    }

    if (started) {
      return true;
    }

    const auto md_core = infra::topology::find_core(topology, to_node_id(NodeId::Md));
    const auto strat_core = infra::topology::find_core(topology, to_node_id(NodeId::Strat));
    const auto or_core = infra::topology::find_core(topology, to_node_id(NodeId::Or));

    if (!md_core || !strat_core || !or_core) {
      error = "missing core mapping";
      return false;
    }

    running.store(true, std::memory_order_release);

    std::vector<infra::topology::WorkerSpec> workers;
    workers.push_back(infra::topology::WorkerSpec {.name = "md", .core = *md_core, .run = [this]() { md->run(); }});
    workers.push_back(
        infra::topology::WorkerSpec {.name = "strat", .core = *strat_core, .run = [this]() { strat->run(); }});
    workers.push_back(infra::topology::WorkerSpec {.name = "or", .core = *or_core, .run = [this]() { orr->run(); }});

    if (!thread_runtime.launch(workers)) {
      running.store(false, std::memory_order_release);
      error = thread_runtime.last_error();
      return false;
    }

    started = true;
    return true;
  }

  void stop() {
    running.store(false, std::memory_order_release);
  }

  void join() {
    thread_runtime.join();
    started = false;
  }

  [[nodiscard]] bool try_push_tick(const Tick& tick) noexcept {
    return driver_tick_tx.try_send(tick);
  }

  [[nodiscard]] StatsSnapshot snapshot() const {
    return StatsSnapshot {
        .md_ticks_sent = stats.md_ticks_sent.load(std::memory_order_relaxed),
        .strat_ticks_seen = stats.strat_ticks_seen.load(std::memory_order_relaxed),
        .strat_orders_sent = stats.strat_orders_sent.load(std::memory_order_relaxed),
        .strat_acks_seen = stats.strat_acks_seen.load(std::memory_order_relaxed),
        .or_orders_seen = stats.or_orders_seen.load(std::memory_order_relaxed),
        .or_acks_sent = stats.or_acks_sent.load(std::memory_order_relaxed),
        .tick_one_way_count = stats.tick_one_way_count.load(std::memory_order_relaxed),
        .tick_one_way_sum_ns = stats.tick_one_way_sum_ns.load(std::memory_order_relaxed),
        .tick_one_way_max_ns = stats.tick_one_way_max_ns.load(std::memory_order_relaxed),
        .order_rtt_count = stats.order_rtt_count.load(std::memory_order_relaxed),
        .order_rtt_sum_ns = stats.order_rtt_sum_ns.load(std::memory_order_relaxed),
        .order_rtt_max_ns = stats.order_rtt_max_ns.load(std::memory_order_relaxed),
        .trace_ticks_seen = stats.trace_ticks_seen.load(std::memory_order_relaxed),
        .trace_acks_seen = stats.trace_acks_seen.load(std::memory_order_relaxed),
    };
  }
};

double StatsSnapshot::tick_one_way_avg_ns() const noexcept {
  if (tick_one_way_count == 0U) {
    return 0.0;
  }
  return static_cast<double>(tick_one_way_sum_ns) / static_cast<double>(tick_one_way_count);
}

double StatsSnapshot::order_rtt_avg_ns() const noexcept {
  if (order_rtt_count == 0U) {
    return 0.0;
  }
  return static_cast<double>(order_rtt_sum_ns) / static_cast<double>(order_rtt_count);
}

Runtime::Runtime(infra::topology::Topology topology, MvpConfig config)
    : impl_(std::make_unique<Impl>(std::move(topology), config)) {}

Runtime::~Runtime() {
  stop();
  join();
}

bool Runtime::build() {
  return impl_->build();
}

bool Runtime::start() {
  return impl_->start();
}

void Runtime::stop() {
  impl_->stop();
}

void Runtime::join() {
  impl_->join();
}

bool Runtime::try_push_tick(const Tick& tick) noexcept {
  return impl_->try_push_tick(tick);
}

StatsSnapshot Runtime::snapshot() const {
  return impl_->snapshot();
}

const std::string& Runtime::last_error() const noexcept {
  return impl_->error;
}

RunResult run_for(std::chrono::milliseconds duration, const MvpConfig& config) {
  RunResult result;

  Runtime runtime(make_topology(config), config);
  result.built = runtime.build();
  if (!result.built) {
    result.error = runtime.last_error();
    return result;
  }

  result.started = runtime.start();
  if (!result.started) {
    result.error = runtime.last_error();
    return result;
  }

  std::atomic<bool> feed_running {true};
  std::thread feeder([&]() {
    u64 seq = 1;
    auto next_emit = std::chrono::steady_clock::now();

    while (feed_running.load(std::memory_order_relaxed)) {
      Tick tick {
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
  runtime.stop();
  if (feeder.joinable()) {
    feeder.join();
  }
  runtime.join();

  result.stats = runtime.snapshot();
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
