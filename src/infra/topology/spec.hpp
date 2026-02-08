#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace magus2::infra::topology {

using NodeId = std::uint16_t;
using ContractId = std::uint16_t;

enum class Direction : std::uint8_t {
  Rx,
  Tx,
};

struct PortSpec {
  std::string_view name;
  Direction direction;
  ContractId contract;
  bool required;
};

struct NodeSpec {
  NodeId id;
  std::string_view name;
  std::uint32_t core;
  std::vector<PortSpec> ports;
};

struct EdgeSpec {
  NodeId from;
  std::string_view from_port;
  NodeId to;
  std::string_view to_port;
  ContractId contract;
  std::size_t depth;
};

struct Topology {
  std::vector<NodeSpec> nodes;
  std::vector<EdgeSpec> edges;
};

[[nodiscard]] inline PortSpec rx_port(std::string_view name, ContractId contract, bool required = true) {
  return PortSpec {.name = name, .direction = Direction::Rx, .contract = contract, .required = required};
}

[[nodiscard]] inline PortSpec tx_port(std::string_view name, ContractId contract, bool required = true) {
  return PortSpec {.name = name, .direction = Direction::Tx, .contract = contract, .required = required};
}

[[nodiscard]] inline NodeSpec node_spec(
    NodeId id, std::string_view name, std::uint32_t core, std::initializer_list<PortSpec> ports) {
  return NodeSpec {.id = id, .name = name, .core = core, .ports = std::vector<PortSpec>(ports)};
}

[[nodiscard]] inline EdgeSpec edge_spec(NodeId from,
    std::string_view from_port,
    NodeId to,
    std::string_view to_port,
    ContractId contract,
    std::size_t depth) {
  return EdgeSpec {
      .from = from,
      .from_port = from_port,
      .to = to,
      .to_port = to_port,
      .contract = contract,
      .depth = depth,
  };
}

[[nodiscard]] inline Topology make_topology(
    std::initializer_list<NodeSpec> nodes, std::initializer_list<EdgeSpec> edges) {
  return Topology {.nodes = std::vector<NodeSpec>(nodes), .edges = std::vector<EdgeSpec>(edges)};
}

[[nodiscard]] inline std::optional<std::size_t> find_edge_index(
    const Topology& topology, NodeId node, std::string_view port_name, Direction direction, ContractId contract) {
  for (std::size_t i = 0; i < topology.edges.size(); ++i) {
    const EdgeSpec& edge = topology.edges[i];
    if (edge.contract != contract) {
      continue;
    }

    if (direction == Direction::Tx) {
      if (edge.from == node && edge.from_port == port_name) {
        return i;
      }
    } else {
      if (edge.to == node && edge.to_port == port_name) {
        return i;
      }
    }
  }
  return std::nullopt;
}

[[nodiscard]] inline std::optional<std::uint32_t> find_core(const Topology& topology, NodeId node_id) {
  for (const NodeSpec& node : topology.nodes) {
    if (node.id == node_id) {
      return node.core;
    }
  }
  return std::nullopt;
}

[[nodiscard]] inline bool validate_ports(const Topology& topology, std::string& error) {
  for (const NodeSpec& node : topology.nodes) {
    for (const PortSpec& port : node.ports) {
      std::size_t matches = 0;
      for (const EdgeSpec& edge : topology.edges) {
        if (edge.contract != port.contract) {
          continue;
        }

        if (port.direction == Direction::Tx && edge.from == node.id && edge.from_port == port.name) {
          ++matches;
        }
        if (port.direction == Direction::Rx && edge.to == node.id && edge.to_port == port.name) {
          ++matches;
        }
      }

      if (port.required && matches != 1U) {
        error = "required port wiring mismatch for node=" + std::string(node.name) + " port=" + std::string(port.name);
        return false;
      }

      if (!port.required && matches > 1U) {
        error = "optional port wired multiple times for node=" + std::string(node.name) + " port=" + std::string(port.name);
        return false;
      }
    }
  }

  return true;
}

}  // namespace magus2::infra::topology
