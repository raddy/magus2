#pragma once

#include "mvp/contracts.hpp"
#include "mvp/stats.hpp"

#include <infra/topology/ports.hpp>

namespace magus2::mvp {

template<typename Envelope>
using Inbox = infra::topology::Inbox<Envelope>;

template<typename Envelope>
using Outbox = infra::topology::Outbox<Envelope>;

// App-specific boundary: these bundles name concrete node roles and bind generic
// infra inbox/outbox endpoints to concrete envelope contracts.
struct IngressNodePorts {
  Outbox<TickEnvelope> tick_tx;
};

struct MdNodePorts {
  Inbox<TickEnvelope> tick_rx;
  Outbox<TickEnvelope> tick_tx;
};

struct StrategyNodePorts {
  Inbox<TickEnvelope> tick_rx;
  Outbox<OrderReqEnvelope> order_tx;
  Inbox<OrderAckEnvelope> ack_rx;
};

struct OrderRouterNodePorts {
  Inbox<OrderReqEnvelope> order_rx;
  Outbox<OrderAckEnvelope> ack_tx;
};

class MdNode {
public:
  MdNode(MdNodePorts ports, std::atomic<bool>& running, RuntimeCounters& stats, u16 trace_thread_idx);
  void run();

private:
  MdNodePorts ports_;
  std::atomic<bool>& running_;
  RuntimeCounters& stats_;
  u16 trace_thread_idx_;
};

class StratNode {
public:
  StratNode(
      StrategyNodePorts ports,
      std::atomic<bool>& running,
      RuntimeCounters& stats,
      u64 order_every_n_ticks,
      u16 trace_thread_idx);
  void run();

private:
  StrategyNodePorts ports_;
  std::atomic<bool>& running_;
  RuntimeCounters& stats_;
  u64 order_every_n_ticks_;
  u16 trace_thread_idx_;
};

class OrNode {
public:
  OrNode(OrderRouterNodePorts ports, std::atomic<bool>& running, RuntimeCounters& stats, u16 trace_thread_idx);
  void run();

private:
  OrderRouterNodePorts ports_;
  std::atomic<bool>& running_;
  RuntimeCounters& stats_;
  u16 trace_thread_idx_;
};

}  // namespace magus2::mvp
