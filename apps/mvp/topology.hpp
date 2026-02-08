#pragma once

#include "mvp/contracts.hpp"

#include <infra/types/short_types.hpp>
#include <infra/topology/spec.hpp>

namespace magus2::mvp {

// App-specific boundary: this file declares the MVP graph (nodes + edges + depths + cores).
// infra::topology::Topology itself stays generic.
struct MvpConfig {
  infra::u32 md_core {0};
  infra::u32 strat_core {1};
  infra::u32 or_core {2};

  std::size_t ingress_depth {64};
  std::size_t tick_depth {64};
  std::size_t order_depth {32};
  std::size_t ack_depth {32};
  std::size_t queue_arena_bytes {1U << 20U};

  infra::u64 tick_interval_us {50};
  infra::u64 order_every_n_ticks {8};
};

[[nodiscard]] inline infra::topology::ContractId to_contract_id(Contract contract) {
  return static_cast<infra::topology::ContractId>(contract);
}

[[nodiscard]] inline infra::topology::NodeId to_node_id(NodeId node) {
  return static_cast<infra::topology::NodeId>(node);
}

inline infra::topology::Topology make_topology(const MvpConfig& cfg) {
  using infra::topology::edge_spec;
  using infra::topology::make_topology;
  using infra::topology::node_spec;
  using infra::topology::rx_port;
  using infra::topology::tx_port;

  return make_topology(
      {
          node_spec(
              to_node_id(NodeId::Md),
              "md",
              cfg.md_core,
              {rx_port("tick_rx", to_contract_id(Contract::Tick)), tx_port("tick_tx", to_contract_id(Contract::Tick))}),
          node_spec(
              to_node_id(NodeId::Strat),
              "strat",
              cfg.strat_core,
              {rx_port("tick_rx", to_contract_id(Contract::Tick)),
                  tx_port("order_tx", to_contract_id(Contract::OrderReq)),
                  rx_port("ack_rx", to_contract_id(Contract::OrderAck))}),
          node_spec(
              to_node_id(NodeId::Or),
              "or",
              cfg.or_core,
              {rx_port("order_rx", to_contract_id(Contract::OrderReq)),
                  tx_port("ack_tx", to_contract_id(Contract::OrderAck))}),
      },
      {
          edge_spec(
              to_node_id(NodeId::Ingress),
              "ingress_tick_tx",
              to_node_id(NodeId::Md),
              "tick_rx",
              to_contract_id(Contract::Tick),
              cfg.ingress_depth),
          edge_spec(
              to_node_id(NodeId::Md),
              "tick_tx",
              to_node_id(NodeId::Strat),
              "tick_rx",
              to_contract_id(Contract::Tick),
              cfg.tick_depth),
          edge_spec(
              to_node_id(NodeId::Strat),
              "order_tx",
              to_node_id(NodeId::Or),
              "order_rx",
              to_contract_id(Contract::OrderReq),
              cfg.order_depth),
          edge_spec(
              to_node_id(NodeId::Or),
              "ack_tx",
              to_node_id(NodeId::Strat),
              "ack_rx",
              to_contract_id(Contract::OrderAck),
              cfg.ack_depth),
      });
}

}  // namespace magus2::mvp
