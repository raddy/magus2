#include "mvp/runtime.hpp"

#include <infra/topology/ports.hpp>
#include <infra/topology/runtime.hpp>
#include <infra/topology/spec.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <rigtorp/rigtorp.hpp>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef TLOG_NOFMTLOG
#  include <fmtlog-inl.h>
#endif

namespace magus2::mvp {
namespace {

struct RuntimeStats {
  std::atomic<std::uint64_t> md_ticks_sent {0};
  std::atomic<std::uint64_t> strat_ticks_seen {0};
  std::atomic<std::uint64_t> strat_orders_sent {0};
  std::atomic<std::uint64_t> strat_acks_seen {0};
  std::atomic<std::uint64_t> or_orders_seen {0};
  std::atomic<std::uint64_t> or_acks_sent {0};

  std::atomic<std::uint64_t> tick_one_way_count {0};
  std::atomic<std::uint64_t> tick_one_way_sum_ns {0};
  std::atomic<std::uint64_t> tick_one_way_max_ns {0};

  std::atomic<std::uint64_t> order_rtt_count {0};
  std::atomic<std::uint64_t> order_rtt_sum_ns {0};
  std::atomic<std::uint64_t> order_rtt_max_ns {0};

  std::atomic<std::uint64_t> trace_ticks_seen {0};
  std::atomic<std::uint64_t> trace_acks_seen {0};
};

inline void update_max(std::atomic<std::uint64_t>& target, std::uint64_t value) noexcept {
  std::uint64_t current = target.load(std::memory_order_relaxed);
  while (current < value
         && !target.compare_exchange_weak(current, value, std::memory_order_relaxed, std::memory_order_relaxed)) {}
}

inline bool has_trace_id(const tlog::carrier& carrier) noexcept {
  return carrier.c.tid.hi != 0U || carrier.c.tid.lo != 0U;
}

struct MdPorts {
  infra::topology::TxPort<Tick> tick_tx;
};

struct StratPorts {
  infra::topology::RxPort<Tick> tick_rx;
  infra::topology::TxPort<OrderReq> order_tx;
  infra::topology::RxPort<OrderAck> ack_rx;
};

struct OrPorts {
  infra::topology::RxPort<OrderReq> order_rx;
  infra::topology::TxPort<OrderAck> ack_tx;
};

class MdNode {
public:
  MdNode(MdPorts ports, std::atomic<bool>& running, RuntimeStats& stats, std::uint64_t tick_interval_us)
      : ports_(ports)
      , running_(running)
      , stats_(stats)
      , tick_interval_us_(tick_interval_us == 0U ? 1U : tick_interval_us) {}

  void run() {
    tlog::init_thread(0);
    std::uint64_t seq = 1;
    auto next_emit = std::chrono::steady_clock::now();

    while (running_.load(std::memory_order_relaxed)) {
      tlog::ingress(TLOG_KEY("tick_seq"), seq);
      tlog::scope_span span {};
      Tick tick {
          .seq = seq,
          .ts_ns = infra::topology::monotonic_ns(),
          .ctx = tlog::carry(),
      };
      if (seq <= 3U) {
        TLOGI("event=md.tick seq={}", seq);
      }
      if (ports_.tick_tx.try_send(tick)) {
        ++seq;
        stats_.md_ticks_sent.fetch_add(1, std::memory_order_relaxed);

        next_emit += std::chrono::microseconds(tick_interval_us_);
        const auto now = std::chrono::steady_clock::now();
        if (next_emit > now) {
          std::this_thread::sleep_until(next_emit);
        } else {
          next_emit = now;
        }
      } else {
        infra::topology::relax_cpu();
      }
    }
  }

private:
  MdPorts ports_;
  std::atomic<bool>& running_;
  RuntimeStats& stats_;
  std::uint64_t tick_interval_us_;
};

class StratNode {
public:
  StratNode(StratPorts ports, std::atomic<bool>& running, RuntimeStats& stats, std::uint64_t order_every_n_ticks)
      : ports_(ports)
      , running_(running)
      , stats_(stats)
      , order_every_n_ticks_(order_every_n_ticks == 0U ? 1U : order_every_n_ticks) {}

  void run() {
    tlog::init_thread(1);
    std::uint64_t tick_count = 0;
    std::uint32_t order_id = 0;

    while (running_.load(std::memory_order_relaxed)) {
      bool processed = false;

      Tick tick {};
      while (ports_.tick_rx.try_recv(tick)) {
        tlog::scope_adopt adopted {tick.ctx};
        tlog::scope_span span {};
        const std::uint64_t now_ns = infra::topology::monotonic_ns();
        if (tick.seq <= 3U) {
          TLOGI("event=strat.tick seq={}", tick.seq);
        }
        ++tick_count;
        stats_.strat_ticks_seen.fetch_add(1, std::memory_order_relaxed);
        if (has_trace_id(tick.ctx)) {
          stats_.trace_ticks_seen.fetch_add(1, std::memory_order_relaxed);
        }
        processed = true;

        if (now_ns >= tick.ts_ns) {
          const std::uint64_t one_way_ns = now_ns - tick.ts_ns;
          stats_.tick_one_way_count.fetch_add(1, std::memory_order_relaxed);
          stats_.tick_one_way_sum_ns.fetch_add(one_way_ns, std::memory_order_relaxed);
          update_max(stats_.tick_one_way_max_ns, one_way_ns);
        }

        if ((tick_count % order_every_n_ticks_) == 0U) {
          const OrderReq req {
              .order_id = ++order_id,
              .instr_id = 1,
              .send_ts_ns = now_ns,
              .ctx = tlog::carry(),
              .px = 10000,
              .qty = 1,
              .side = 1,
              .pad = {0, 0, 0},
          };
          if (req.order_id <= 3U) {
            TLOGI("event=strat.order_send order_id={}", req.order_id);
          }

          while (running_.load(std::memory_order_relaxed) && !ports_.order_tx.try_send(req)) {
            infra::topology::relax_cpu();
          }

          if (running_.load(std::memory_order_relaxed)) {
            stats_.strat_orders_sent.fetch_add(1, std::memory_order_relaxed);
          }
        }
      }

      OrderAck ack {};
      while (ports_.ack_rx.try_recv(ack)) {
        tlog::scope_adopt adopted {ack.ctx};
        tlog::scope_span span {};
        const std::uint64_t now_ns = infra::topology::monotonic_ns();
        if (ack.order_id <= 3U) {
          TLOGI("event=strat.ack_recv order_id={}", ack.order_id);
        }
        stats_.strat_acks_seen.fetch_add(1, std::memory_order_relaxed);
        if (has_trace_id(ack.ctx)) {
          stats_.trace_acks_seen.fetch_add(1, std::memory_order_relaxed);
        }
        processed = true;

        if (now_ns >= ack.origin_ts_ns) {
          const std::uint64_t rtt_ns = now_ns - ack.origin_ts_ns;
          stats_.order_rtt_count.fetch_add(1, std::memory_order_relaxed);
          stats_.order_rtt_sum_ns.fetch_add(rtt_ns, std::memory_order_relaxed);
          update_max(stats_.order_rtt_max_ns, rtt_ns);
        }
      }

      if (!processed) {
        infra::topology::relax_cpu();
      }
    }
  }

private:
  StratPorts ports_;
  std::atomic<bool>& running_;
  RuntimeStats& stats_;
  std::uint64_t order_every_n_ticks_;
};

class OrNode {
public:
  OrNode(OrPorts ports, std::atomic<bool>& running, RuntimeStats& stats)
      : ports_(ports)
      , running_(running)
      , stats_(stats) {}

  void run() {
    tlog::init_thread(2);
    while (running_.load(std::memory_order_relaxed)) {
      bool processed = false;

      OrderReq req {};
      while (ports_.order_rx.try_recv(req)) {
        tlog::scope_adopt adopted {req.ctx};
        tlog::scope_span span {};
        if (req.order_id <= 3U) {
          TLOGI("event=or.order_recv order_id={}", req.order_id);
        }
        stats_.or_orders_seen.fetch_add(1, std::memory_order_relaxed);
        processed = true;

        const OrderAck ack {
            .order_id = req.order_id,
            .origin_ts_ns = req.send_ts_ns,
            .ctx = tlog::carry(),
            .status = 1,
            .pad = {0, 0, 0},
        };
        if (ack.order_id <= 3U) {
          TLOGI("event=or.ack_send order_id={}", ack.order_id);
        }

        while (running_.load(std::memory_order_relaxed) && !ports_.ack_tx.try_send(ack)) {
          infra::topology::relax_cpu();
        }

        if (running_.load(std::memory_order_relaxed)) {
          stats_.or_acks_sent.fetch_add(1, std::memory_order_relaxed);
        }
      }

      if (!processed) {
        infra::topology::relax_cpu();
      }
    }
  }

private:
  OrPorts ports_;
  std::atomic<bool>& running_;
  RuntimeStats& stats_;
};

struct QueueBase {
  virtual ~QueueBase() = default;
};

template<typename T>
struct QueueStorage final : QueueBase {
  explicit QueueStorage(std::size_t depth)
      : queue(depth) {}

  rigtorp::SPSCQueue<T> queue;
};

}  // namespace

struct Runtime::Impl {
  explicit Impl(infra::topology::Topology topology, MvpConfig config)
      : topology(std::move(topology))
      , config(config) {}

  infra::topology::Topology topology;
  MvpConfig config;

  std::vector<std::unique_ptr<QueueBase>> queue_storage;
  infra::topology::ThreadRuntime thread_runtime;

  RuntimeStats stats;
  std::atomic<bool> running {false};

  std::optional<MdNode> md;
  std::optional<StratNode> strat;
  std::optional<OrNode> orr;

  bool built {false};
  bool started {false};
  std::string error;

  template<typename T>
  [[nodiscard]] rigtorp::SPSCQueue<T>* queue_as(std::size_t edge_index) {
    auto* typed = dynamic_cast<QueueStorage<T>*>(queue_storage[edge_index].get());
    return typed == nullptr ? nullptr : &typed->queue;
  }

  [[nodiscard]] bool build() {
    if (built) {
      return true;
    }

    if (topology.nodes.empty() || topology.edges.empty()) {
      error = "topology is empty";
      return false;
    }

    if (!infra::topology::validate_ports(topology, error)) {
      return false;
    }

    queue_storage.clear();
    queue_storage.reserve(topology.edges.size());

    for (const infra::topology::EdgeSpec& edge : topology.edges) {
      if (edge.depth < 2U) {
        error = "edge depth must be >=2";
        return false;
      }

      switch (static_cast<Contract>(edge.contract)) {
        case Contract::Tick:
          queue_storage.push_back(std::make_unique<QueueStorage<Tick>>(edge.depth));
          break;
        case Contract::OrderReq:
          queue_storage.push_back(std::make_unique<QueueStorage<OrderReq>>(edge.depth));
          break;
        case Contract::OrderAck:
          queue_storage.push_back(std::make_unique<QueueStorage<OrderAck>>(edge.depth));
          break;
      }
    }

    MdPorts md_ports;
    StratPorts strat_ports;
    OrPorts or_ports;

    const auto md_tick_tx = infra::topology::find_edge_index(
        topology, to_node_id(NodeId::Md), "tick_tx", infra::topology::Direction::Tx, to_contract_id(Contract::Tick));
    const auto strat_tick_rx = infra::topology::find_edge_index(
        topology, to_node_id(NodeId::Strat), "tick_rx", infra::topology::Direction::Rx, to_contract_id(Contract::Tick));
    const auto strat_order_tx = infra::topology::find_edge_index(
        topology,
        to_node_id(NodeId::Strat),
        "order_tx",
        infra::topology::Direction::Tx,
        to_contract_id(Contract::OrderReq));
    const auto or_order_rx = infra::topology::find_edge_index(
        topology, to_node_id(NodeId::Or), "order_rx", infra::topology::Direction::Rx, to_contract_id(Contract::OrderReq));
    const auto or_ack_tx = infra::topology::find_edge_index(
        topology, to_node_id(NodeId::Or), "ack_tx", infra::topology::Direction::Tx, to_contract_id(Contract::OrderAck));
    const auto strat_ack_rx = infra::topology::find_edge_index(
        topology,
        to_node_id(NodeId::Strat),
        "ack_rx",
        infra::topology::Direction::Rx,
        to_contract_id(Contract::OrderAck));

    if (!md_tick_tx || !strat_tick_rx || !strat_order_tx || !or_order_rx || !or_ack_tx || !strat_ack_rx) {
      error = "failed to resolve one or more required ports";
      return false;
    }

    md_ports.tick_tx.q = queue_as<Tick>(*md_tick_tx);
    strat_ports.tick_rx.q = queue_as<Tick>(*strat_tick_rx);
    strat_ports.order_tx.q = queue_as<OrderReq>(*strat_order_tx);
    or_ports.order_rx.q = queue_as<OrderReq>(*or_order_rx);
    or_ports.ack_tx.q = queue_as<OrderAck>(*or_ack_tx);
    strat_ports.ack_rx.q = queue_as<OrderAck>(*strat_ack_rx);

    if (!md_ports.tick_tx.present() || !strat_ports.tick_rx.present() || !strat_ports.order_tx.present()
        || !or_ports.order_rx.present() || !or_ports.ack_tx.present() || !strat_ports.ack_rx.present()) {
      error = "failed to bind one or more queue endpoints";
      return false;
    }

    md.emplace(md_ports, running, stats, config.md_tick_interval_us);
    strat.emplace(strat_ports, running, stats, config.order_every_n_ticks);
    orr.emplace(or_ports, running, stats);

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
      error = "missing node core mapping";
      return false;
    }

    running.store(true, std::memory_order_release);

    const std::vector<infra::topology::WorkerSpec> workers {
        infra::topology::WorkerSpec {.name = "md", .core = *md_core, .run = [this]() { md->run(); }},
        infra::topology::WorkerSpec {.name = "strat", .core = *strat_core, .run = [this]() { strat->run(); }},
        infra::topology::WorkerSpec {.name = "or", .core = *or_core, .run = [this]() { orr->run(); }},
    };

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

  std::this_thread::sleep_for(duration);
  runtime.stop();
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
