#pragma once

#include <cstdint>
#include <type_traits>

#include <tlog/tlog.hpp>

namespace magus2::mvp {

enum class Contract : std::uint16_t {
  Tick = 1,
  OrderReq = 2,
  OrderAck = 3,
};

enum class NodeId : std::uint16_t {
  Md = 1,
  Strat = 2,
  Or = 3,
};

struct Tick {
  std::uint64_t seq;
  std::uint64_t ts_ns;
  tlog::carrier ctx;
};

struct OrderReq {
  std::uint32_t order_id;
  std::uint32_t instr_id;
  std::uint64_t send_ts_ns;
  tlog::carrier ctx;
  std::int64_t px;
  std::int32_t qty;
  std::uint8_t side;
  std::uint8_t pad[3];
};

struct OrderAck {
  std::uint32_t order_id;
  std::uint64_t origin_ts_ns;
  tlog::carrier ctx;
  std::uint8_t status;
  std::uint8_t pad[3];
};

static_assert(std::is_standard_layout_v<Tick> && std::is_trivially_copyable_v<Tick>);
static_assert(std::is_standard_layout_v<OrderReq> && std::is_trivially_copyable_v<OrderReq>);
static_assert(std::is_standard_layout_v<OrderAck> && std::is_trivially_copyable_v<OrderAck>);

}  // namespace magus2::mvp
