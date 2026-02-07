#pragma once

#include "mvp/contracts.hpp"

#include <infra/topology/spec.hpp>

#include <cstddef>
#include <cstdint>

namespace magus2::mvp {

struct MvpConfig {
  std::uint32_t md_core {0};
  std::uint32_t strat_core {1};
  std::uint32_t or_core {2};

  std::size_t tick_depth {64};
  std::size_t order_depth {32};
  std::size_t ack_depth {32};

  std::uint64_t md_tick_interval_us {50};
  std::uint64_t order_every_n_ticks {8};
};

[[nodiscard]] inline infra::topology::ContractId to_contract_id(Contract contract) {
  return static_cast<infra::topology::ContractId>(contract);
}

[[nodiscard]] inline infra::topology::NodeId to_node_id(NodeId node) {
  return static_cast<infra::topology::NodeId>(node);
}

inline infra::topology::Topology make_topology(const MvpConfig& cfg) {
  using infra::topology::Direction;
  using infra::topology::EdgeSpec;
  using infra::topology::NodeSpec;
  using infra::topology::PortSpec;
  using infra::topology::Topology;

  Topology topology;

  topology.nodes.push_back(NodeSpec {
      .id = to_node_id(NodeId::Md),
      .name = "md",
      .core = cfg.md_core,
      .ports = {{"tick_tx", Direction::Tx, to_contract_id(Contract::Tick), true}},
  });

  topology.nodes.push_back(NodeSpec {
      .id = to_node_id(NodeId::Strat),
      .name = "strat",
      .core = cfg.strat_core,
      .ports = {
          {"tick_rx", Direction::Rx, to_contract_id(Contract::Tick), true},
          {"order_tx", Direction::Tx, to_contract_id(Contract::OrderReq), true},
          {"ack_rx", Direction::Rx, to_contract_id(Contract::OrderAck), true},
      },
  });

  topology.nodes.push_back(NodeSpec {
      .id = to_node_id(NodeId::Or),
      .name = "or",
      .core = cfg.or_core,
      .ports = {
          {"order_rx", Direction::Rx, to_contract_id(Contract::OrderReq), true},
          {"ack_tx", Direction::Tx, to_contract_id(Contract::OrderAck), true},
      },
  });

  topology.edges.push_back(EdgeSpec {
      .from = to_node_id(NodeId::Md),
      .from_port = "tick_tx",
      .to = to_node_id(NodeId::Strat),
      .to_port = "tick_rx",
      .contract = to_contract_id(Contract::Tick),
      .depth = cfg.tick_depth,
  });

  topology.edges.push_back(EdgeSpec {
      .from = to_node_id(NodeId::Strat),
      .from_port = "order_tx",
      .to = to_node_id(NodeId::Or),
      .to_port = "order_rx",
      .contract = to_contract_id(Contract::OrderReq),
      .depth = cfg.order_depth,
  });

  topology.edges.push_back(EdgeSpec {
      .from = to_node_id(NodeId::Or),
      .from_port = "ack_tx",
      .to = to_node_id(NodeId::Strat),
      .to_port = "ack_rx",
      .contract = to_contract_id(Contract::OrderAck),
      .depth = cfg.ack_depth,
  });

  return topology;
}

}  // namespace magus2::mvp
