#pragma once

#include "mvp/contracts.hpp"

#include <infra/topology/ports.hpp>

#include <atomic>

namespace magus2::mvp {

struct RuntimeStats {
  std::atomic<u64> md_ticks_sent {0};
  std::atomic<u64> strat_ticks_seen {0};
  std::atomic<u64> strat_orders_sent {0};
  std::atomic<u64> strat_acks_seen {0};
  std::atomic<u64> or_orders_seen {0};
  std::atomic<u64> or_acks_sent {0};

  std::atomic<u64> tick_one_way_count {0};
  std::atomic<u64> tick_one_way_sum_ns {0};
  std::atomic<u64> tick_one_way_max_ns {0};

  std::atomic<u64> order_rtt_count {0};
  std::atomic<u64> order_rtt_sum_ns {0};
  std::atomic<u64> order_rtt_max_ns {0};

  std::atomic<u64> trace_ticks_seen {0};
  std::atomic<u64> trace_acks_seen {0};
};

struct MdPorts {
  infra::topology::RxPort<Tick> tick_rx;
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
  MdNode(MdPorts ports, std::atomic<bool>& running, RuntimeStats& stats, u16 trace_thread_idx);
  void run();

private:
  MdPorts ports_;
  std::atomic<bool>& running_;
  RuntimeStats& stats_;
  u16 trace_thread_idx_;
};

class StratNode {
public:
  StratNode(StratPorts ports, std::atomic<bool>& running, RuntimeStats& stats, u64 order_every_n_ticks, u16 trace_thread_idx);
  void run();

private:
  StratPorts ports_;
  std::atomic<bool>& running_;
  RuntimeStats& stats_;
  u64 order_every_n_ticks_;
  u16 trace_thread_idx_;
};

class OrNode {
public:
  OrNode(OrPorts ports, std::atomic<bool>& running, RuntimeStats& stats, u16 trace_thread_idx);
  void run();

private:
  OrPorts ports_;
  std::atomic<bool>& running_;
  RuntimeStats& stats_;
  u16 trace_thread_idx_;
};

}  // namespace magus2::mvp
