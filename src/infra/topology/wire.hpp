#pragma once

#include "infra/topology/ports.hpp"
#include "infra/topology/spec.hpp"

#include <string_view>
#include <tuple>
#include <type_traits>

namespace magus2::infra::topology {

template<typename PortsT, typename EnvelopeT>
struct RxBinding {
  Inbox<EnvelopeT> PortsT::*member;
  NodeId node_id;
  std::string_view port_name;
  ContractId contract_id;
};

template<typename PortsT, typename EnvelopeT>
struct TxBinding {
  Outbox<EnvelopeT> PortsT::*member;
  NodeId node_id;
  std::string_view port_name;
  ContractId contract_id;
};

template<typename PortsT, typename EnvelopeT>
[[nodiscard]] constexpr auto rx_binding(
    Inbox<EnvelopeT> PortsT::*member, NodeId node_id, std::string_view port_name, ContractId contract_id) {
  return RxBinding<PortsT, EnvelopeT> {
      .member = member,
      .node_id = node_id,
      .port_name = port_name,
      .contract_id = contract_id,
  };
}

template<typename PortsT, typename EnvelopeT>
[[nodiscard]] constexpr auto tx_binding(
    Outbox<EnvelopeT> PortsT::*member, NodeId node_id, std::string_view port_name, ContractId contract_id) {
  return TxBinding<PortsT, EnvelopeT> {
      .member = member,
      .node_id = node_id,
      .port_name = port_name,
      .contract_id = contract_id,
  };
}

template<typename EngineT, typename PortsT, typename EnvelopeT>
[[nodiscard]] bool bind_one(EngineT& engine, PortsT& ports, const RxBinding<PortsT, EnvelopeT>& binding) {
  return engine.template bind_rx<EnvelopeT>(
      binding.node_id, binding.port_name, binding.contract_id, ports.*(binding.member));
}

template<typename EngineT, typename PortsT, typename EnvelopeT>
[[nodiscard]] bool bind_one(EngineT& engine, PortsT& ports, const TxBinding<PortsT, EnvelopeT>& binding) {
  return engine.template bind_tx<EnvelopeT>(
      binding.node_id, binding.port_name, binding.contract_id, ports.*(binding.member));
}

template<typename EngineT, typename PortsT, typename... Bindings>
[[nodiscard]] bool bind_all(EngineT& engine, PortsT& ports, const std::tuple<Bindings...>& bindings) {
  bool ok = true;
  std::apply([&](const auto&... binding) { ((ok = ok && bind_one(engine, ports, binding)), ...); }, bindings);
  return ok;
}

template<typename NodeT>
struct WorkerBinding {
  NodeId node_id;
  std::string_view worker_name;
  NodeT* node;
};

template<typename NodeT>
[[nodiscard]] constexpr auto worker_binding(NodeId node_id, std::string_view worker_name, NodeT& node) {
  return WorkerBinding<NodeT> {.node_id = node_id, .worker_name = worker_name, .node = &node};
}

template<typename EngineT, typename NodeT>
[[nodiscard]] bool register_worker(EngineT& engine, const WorkerBinding<NodeT>& worker) {
  return engine.add_worker(worker.node_id, worker.worker_name, [node = worker.node]() { node->run(); });
}

template<typename EngineT, typename... Workers>
[[nodiscard]] bool register_workers(EngineT& engine, const Workers&... workers) {
  static_assert((std::is_class_v<Workers> && ...));
  bool ok = true;
  ((ok = ok && register_worker(engine, workers)), ...);
  return ok;
}

}  // namespace magus2::infra::topology
