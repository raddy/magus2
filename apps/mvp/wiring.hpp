#pragma once

#include "mvp/contracts.hpp"

#include <infra/topology/ports.hpp>
#include <infra/topology/spec.hpp>

#include <memory>
#include <optional>
#include <rigtorp/rigtorp.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace magus2::mvp::wiring {

struct QueueBase {
  virtual ~QueueBase() = default;
};

template<typename T>
struct QueueStorage final : QueueBase {
  explicit QueueStorage(std::size_t depth)
      : queue(depth) {}

  rigtorp::SPSCQueue<T> queue;
};

struct QueueStore {
  std::vector<std::unique_ptr<QueueBase>> storage;

  bool build(const infra::topology::Topology& topology, std::string& error) {
    storage.clear();
    storage.reserve(topology.edges.size());

    for (const infra::topology::EdgeSpec& edge : topology.edges) {
      if (edge.depth < 2U) {
        error = "edge depth must be >= 2";
        return false;
      }

      switch (static_cast<Contract>(edge.contract)) {
        case Contract::Tick:
          storage.push_back(std::make_unique<QueueStorage<Tick>>(edge.depth));
          break;
        case Contract::OrderReq:
          storage.push_back(std::make_unique<QueueStorage<OrderReq>>(edge.depth));
          break;
        case Contract::OrderAck:
          storage.push_back(std::make_unique<QueueStorage<OrderAck>>(edge.depth));
          break;
      }
    }

    return true;
  }

  template<typename T>
  [[nodiscard]] rigtorp::SPSCQueue<T>* queue_as(std::size_t edge_index) {
    auto* typed = dynamic_cast<QueueStorage<T>*>(storage[edge_index].get());
    return typed == nullptr ? nullptr : &typed->queue;
  }
};

template<typename Port>
struct PortMsgType;

template<typename T>
struct PortMsgType<infra::topology::TxPort<T>> {
  using type = T;
};

template<typename T>
struct PortMsgType<infra::topology::RxPort<T>> {
  using type = T;
};

template<typename PortT>
[[nodiscard]] bool bind_tx_port(PortT& port,
    QueueStore& queue_store,
    const infra::topology::Topology& topology,
    NodeId node,
    std::string_view port_name,
    Contract contract,
    std::string& error) {
  const auto edge_index = infra::topology::find_edge_index(
      topology,
      to_node_id(node),
      port_name,
      infra::topology::Direction::Tx,
      to_contract_id(contract));
  if (!edge_index) {
    error = "missing tx edge binding for node=" + std::to_string(static_cast<unsigned>(node));
    return false;
  }

  using Msg = typename PortMsgType<PortT>::type;
  port.q = queue_store.queue_as<Msg>(*edge_index);
  if (!port.present()) {
    error = "tx queue type mismatch for node=" + std::to_string(static_cast<unsigned>(node));
    return false;
  }
  return true;
}

template<typename PortT>
[[nodiscard]] bool bind_rx_port(PortT& port,
    QueueStore& queue_store,
    const infra::topology::Topology& topology,
    NodeId node,
    std::string_view port_name,
    Contract contract,
    std::string& error) {
  const auto edge_index = infra::topology::find_edge_index(
      topology,
      to_node_id(node),
      port_name,
      infra::topology::Direction::Rx,
      to_contract_id(contract));
  if (!edge_index) {
    error = "missing rx edge binding for node=" + std::to_string(static_cast<unsigned>(node));
    return false;
  }

  using Msg = typename PortMsgType<PortT>::type;
  port.q = queue_store.queue_as<Msg>(*edge_index);
  if (!port.present()) {
    error = "rx queue type mismatch for node=" + std::to_string(static_cast<unsigned>(node));
    return false;
  }
  return true;
}

[[nodiscard]] inline infra::u16 trace_thread_idx(NodeId node) noexcept {
  return static_cast<infra::u16>(node);
}

}  // namespace magus2::mvp::wiring
