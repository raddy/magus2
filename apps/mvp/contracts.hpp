#pragma once

#include <infra/types/short_types.hpp>

#include <type_traits>

#include <tlog/tlog.hpp>

namespace magus2::mvp {

using infra::i32;
using infra::i64;
using infra::u8;
using infra::u16;
using infra::u32;
using infra::u64;

enum class Contract : u16 {
  Tick = 1,
  OrderReq = 2,
  OrderAck = 3,
};

enum class NodeId : u16 {
  Ingress = 0,
  Md = 1,
  Strat = 2,
  Or = 3,
};

struct TickEnvelope {
  u64 seq;
  u64 ts_ns;
  tlog::carrier ctx;
};

struct OrderReqEnvelope {
  u32 order_id;
  u32 instr_id;
  u64 send_ts_ns;
  tlog::carrier ctx;
  i64 px;
  i32 qty;
  u8 side;
  u8 pad[3];
};

struct OrderAckEnvelope {
  u32 order_id;
  u64 origin_ts_ns;
  tlog::carrier ctx;
  u8 status;
  u8 pad[3];
};

static_assert(std::is_standard_layout_v<TickEnvelope> && std::is_trivially_copyable_v<TickEnvelope>);
static_assert(std::is_standard_layout_v<OrderReqEnvelope> && std::is_trivially_copyable_v<OrderReqEnvelope>);
static_assert(std::is_standard_layout_v<OrderAckEnvelope> && std::is_trivially_copyable_v<OrderAckEnvelope>);

}  // namespace magus2::mvp
