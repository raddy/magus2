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
  Driver = 1,
  Md = 2,
  Strat = 3,
  Or = 4,
};

struct Tick {
  u64 seq;
  u64 ts_ns;
  tlog::carrier ctx;
};

struct OrderReq {
  u32 order_id;
  u32 instr_id;
  u64 send_ts_ns;
  tlog::carrier ctx;
  i64 px;
  i32 qty;
  u8 side;
  u8 pad[3];
};

struct OrderAck {
  u32 order_id;
  u64 origin_ts_ns;
  tlog::carrier ctx;
  u8 status;
  u8 pad[3];
};

static_assert(std::is_standard_layout_v<Tick> && std::is_trivially_copyable_v<Tick>);
static_assert(std::is_standard_layout_v<OrderReq> && std::is_trivially_copyable_v<OrderReq>);
static_assert(std::is_standard_layout_v<OrderAck> && std::is_trivially_copyable_v<OrderAck>);

}  // namespace magus2::mvp
