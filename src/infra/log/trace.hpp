#pragma once

#include <infra/types/short_types.hpp>

#include <tlog/tlog.hpp>

namespace magus2::infra::trace {

using Carrier = tlog::carrier;
using AdoptScope = tlog::scope_adopt;
using SpanScope = tlog::scope_span;

inline void thread_init(u16 thread_idx) noexcept {
  tlog::init_thread(thread_idx);
}

inline void ingress_tick_seq(u64 seq) noexcept {
  TINGRESS("tick_seq", seq);
}

inline void ingress_order_id(u64 order_id) noexcept {
  TINGRESS("order_id", order_id);
}

inline void ingress_pending_tick_seq() noexcept {
  TINGRESS_PENDING("tick_seq");
}

inline void set_corr_tick_seq(u64 seq) noexcept {
  tlog::set_corr(TLOG_KEY("tick_seq"), seq);
}

[[nodiscard]] inline Carrier carry() noexcept {
  return tlog::carry();
}

}  // namespace magus2::infra::trace
