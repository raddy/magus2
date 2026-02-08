#include "mvp/nodes.hpp"

#include <infra/log/trace.hpp>
#include <infra/topology/runtime.hpp>

namespace magus2::mvp {

inline void update_max(std::atomic<u64>& target, u64 value) noexcept {
  u64 current = target.load(std::memory_order_relaxed);
  while (current < value
         && !target.compare_exchange_weak(current, value, std::memory_order_relaxed, std::memory_order_relaxed)) {}
}

inline bool has_trace_id(const tlog::carrier& carrier) noexcept {
  return carrier.c.tid.hi != 0U || carrier.c.tid.lo != 0U;
}

MdNode::MdNode(MdNodePorts ports, std::atomic<bool>& running, RuntimeCounters& stats, u16 trace_thread_idx)
    : ports_(ports)
    , running_(running)
    , stats_(stats)
    , trace_thread_idx_(trace_thread_idx) {}

void MdNode::run() {
  infra::trace::thread_init(trace_thread_idx_);

  while (running_.load(std::memory_order_relaxed)) {
    bool processed = false;

    TickEnvelope tick {};
    while (ports_.tick_rx.try_recv(tick)) {
      processed = true;

      if (has_trace_id(tick.ctx)) {
        infra::trace::AdoptScope adopt {tick.ctx};
        infra::trace::SpanScope span {};
        tick.ctx = infra::trace::carry();
      } else {
        infra::trace::ingress_tick_seq(tick.seq);
        infra::trace::SpanScope span {};
        tick.ctx = infra::trace::carry();
      }

      while (running_.load(std::memory_order_relaxed) && !ports_.tick_tx.try_send(tick)) {
        infra::topology::relax_cpu();
      }

      if (running_.load(std::memory_order_relaxed)) {
        stats_.md_ticks_sent.fetch_add(1, std::memory_order_relaxed);
      }
    }

    if (!processed) {
      infra::topology::relax_cpu();
    }
  }
}

StratNode::StratNode(
    StrategyNodePorts ports,
    std::atomic<bool>& running,
    RuntimeCounters& stats,
    u64 order_every_n_ticks,
    u16 trace_thread_idx)
    : ports_(ports)
    , running_(running)
    , stats_(stats)
    , order_every_n_ticks_(order_every_n_ticks == 0U ? 1U : order_every_n_ticks)
    , trace_thread_idx_(trace_thread_idx) {}

void StratNode::run() {
  infra::trace::thread_init(trace_thread_idx_);

  u64 tick_count = 0;
  u32 order_id = 0;

  while (running_.load(std::memory_order_relaxed)) {
    bool processed = false;

    TickEnvelope tick {};
    while (ports_.tick_rx.try_recv(tick)) {
      infra::trace::AdoptScope adopt {tick.ctx};
      infra::trace::SpanScope span {};

      const u64 now_ns = infra::topology::monotonic_ns();
      ++tick_count;
      processed = true;
      stats_.strat_ticks_seen.fetch_add(1, std::memory_order_relaxed);

      if (has_trace_id(tick.ctx)) {
        stats_.trace_ticks_seen.fetch_add(1, std::memory_order_relaxed);
      }

      if (now_ns >= tick.ts_ns) {
        const u64 one_way_ns = now_ns - tick.ts_ns;
        stats_.tick_one_way_count.fetch_add(1, std::memory_order_relaxed);
        stats_.tick_one_way_sum_ns.fetch_add(one_way_ns, std::memory_order_relaxed);
        update_max(stats_.tick_one_way_max_ns, one_way_ns);
      }

      if ((tick_count % order_every_n_ticks_) == 0U) {
        infra::trace::ingress_order_id(static_cast<u64>(order_id + 1U));
        infra::trace::SpanScope order_span {};

        const OrderReqEnvelope req {
            .order_id = ++order_id,
            .instr_id = 1,
            .send_ts_ns = now_ns,
            .ctx = infra::trace::carry(),
            .px = 10000,
            .qty = 1,
            .side = 1,
            .pad = {0, 0, 0},
        };

        while (running_.load(std::memory_order_relaxed) && !ports_.order_tx.try_send(req)) {
          infra::topology::relax_cpu();
        }

        if (running_.load(std::memory_order_relaxed)) {
          stats_.strat_orders_sent.fetch_add(1, std::memory_order_relaxed);
        }
      }
    }

    OrderAckEnvelope ack {};
    while (ports_.ack_rx.try_recv(ack)) {
      infra::trace::AdoptScope adopt {ack.ctx};
      infra::trace::SpanScope span {};

      const u64 now_ns = infra::topology::monotonic_ns();
      processed = true;
      stats_.strat_acks_seen.fetch_add(1, std::memory_order_relaxed);

      if (has_trace_id(ack.ctx)) {
        stats_.trace_acks_seen.fetch_add(1, std::memory_order_relaxed);
      }

      if (now_ns >= ack.origin_ts_ns) {
        const u64 rtt_ns = now_ns - ack.origin_ts_ns;
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

OrNode::OrNode(OrderRouterNodePorts ports, std::atomic<bool>& running, RuntimeCounters& stats, u16 trace_thread_idx)
    : ports_(ports)
    , running_(running)
    , stats_(stats)
    , trace_thread_idx_(trace_thread_idx) {}

void OrNode::run() {
  infra::trace::thread_init(trace_thread_idx_);

  while (running_.load(std::memory_order_relaxed)) {
    bool processed = false;

    OrderReqEnvelope req {};
    while (ports_.order_rx.try_recv(req)) {
      infra::trace::AdoptScope adopt {req.ctx};
      infra::trace::SpanScope span {};

      processed = true;
      stats_.or_orders_seen.fetch_add(1, std::memory_order_relaxed);

      const OrderAckEnvelope ack {
          .order_id = req.order_id,
          .origin_ts_ns = req.send_ts_ns,
          .ctx = infra::trace::carry(),
          .status = 1,
          .pad = {0, 0, 0},
      };

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

}  // namespace magus2::mvp
