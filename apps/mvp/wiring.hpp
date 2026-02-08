#pragma once

#include "mvp/contracts.hpp"
#include "mvp/nodes.hpp"
#include "mvp/topology.hpp"

#include <infra/topology/engine.hpp>
#include <infra/topology/wire.hpp>

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace magus2::mvp::assembly {

// Layer boundary:
// - infra::topology::* below is generic queue/runtime machinery.
// - this file is MVP-specific assembly: concrete node roles, ports, and worker registration.

using QueueStore = infra::topology::QueueStore<
    infra::topology::ContractBinding<Contract::Tick, TickEnvelope>,
    infra::topology::ContractBinding<Contract::OrderReq, OrderReqEnvelope>,
    infra::topology::ContractBinding<Contract::OrderAck, OrderAckEnvelope>>;

using Engine = infra::topology::Engine<QueueStore>;

[[nodiscard]] inline auto ingress_bindings() {
  return std::make_tuple(infra::topology::tx_binding(
      &IngressNodePorts::tick_tx, to_node_id(NodeId::Ingress), "ingress_tick_tx", to_contract_id(Contract::Tick)));
}

[[nodiscard]] inline auto md_bindings() {
  return std::make_tuple(
      infra::topology::rx_binding(
          &MdNodePorts::tick_rx, to_node_id(NodeId::Md), "tick_rx", to_contract_id(Contract::Tick)),
      infra::topology::tx_binding(
          &MdNodePorts::tick_tx, to_node_id(NodeId::Md), "tick_tx", to_contract_id(Contract::Tick)));
}

[[nodiscard]] inline auto strat_bindings() {
  return std::make_tuple(
      infra::topology::rx_binding(
          &StrategyNodePorts::tick_rx, to_node_id(NodeId::Strat), "tick_rx", to_contract_id(Contract::Tick)),
      infra::topology::tx_binding(
          &StrategyNodePorts::order_tx, to_node_id(NodeId::Strat), "order_tx", to_contract_id(Contract::OrderReq)),
      infra::topology::rx_binding(
          &StrategyNodePorts::ack_rx, to_node_id(NodeId::Strat), "ack_rx", to_contract_id(Contract::OrderAck)));
}

[[nodiscard]] inline auto or_bindings() {
  return std::make_tuple(
      infra::topology::rx_binding(
          &OrderRouterNodePorts::order_rx, to_node_id(NodeId::Or), "order_rx", to_contract_id(Contract::OrderReq)),
      infra::topology::tx_binding(
          &OrderRouterNodePorts::ack_tx, to_node_id(NodeId::Or), "ack_tx", to_contract_id(Contract::OrderAck)));
}

[[nodiscard]] inline bool bind_ports(
    Engine& engine,
    IngressNodePorts& ingress,
    MdNodePorts& md,
    StrategyNodePorts& strat,
    OrderRouterNodePorts& order_router) {
  return infra::topology::bind_all(engine, ingress, ingress_bindings())
         && infra::topology::bind_all(engine, md, md_bindings())
         && infra::topology::bind_all(engine, strat, strat_bindings())
         && infra::topology::bind_all(engine, order_router, or_bindings());
}

[[nodiscard]] inline u16 trace_idx_for_node(const Engine& engine, NodeId node_id) {
  const auto core = engine.core(to_node_id(node_id));
  return static_cast<u16>(core.value_or(0));
}

struct AppPortBundle {
  IngressNodePorts ingress;
  MdNodePorts md;
  StrategyNodePorts strat;
  OrderRouterNodePorts order_router;
};

[[nodiscard]] inline bool bind_ports(Engine& engine, AppPortBundle& ports) {
  return bind_ports(engine, ports.ingress, ports.md, ports.strat, ports.order_router);
}

class NodeRegistry {
public:
  class NodeRunner {
  public:
    virtual ~NodeRunner() = default;
    virtual void run() = 0;
  };

  template<typename NodeT, typename... Args>
  NodeRunner* emplace(Args&&... args) {
    auto runner = std::make_unique<NodeRunnerImpl<NodeT>>(std::forward<Args>(args)...);
    NodeRunner* ptr = runner.get();
    runners_.push_back(std::move(runner));
    return ptr;
  }

private:
  template<typename NodeT>
  class NodeRunnerImpl final : public NodeRunner {
  public:
    template<typename... Args>
    explicit NodeRunnerImpl(Args&&... args)
        : node_(std::forward<Args>(args)...) {}

    void run() override {
      node_.run();
    }

  private:
    NodeT node_;
  };

  std::vector<std::unique_ptr<NodeRunner>> runners_;
};

struct NodeFactoryContext {
  Engine& engine;
  std::atomic<bool>& running;
  RuntimeCounters& stats;
  const MvpConfig& config;

  const AppPortBundle& ports;
  NodeRegistry& registry;
};

using NodeFactoryFn = bool (*)(NodeFactoryContext&);

[[nodiscard]] inline bool build_md(NodeFactoryContext& ctx) {
  auto* runner = ctx.registry.emplace<MdNode>(ctx.ports.md, ctx.running, ctx.stats, trace_idx_for_node(ctx.engine, NodeId::Md));
  return ctx.engine.add_worker(to_node_id(NodeId::Md), "md", [runner]() { runner->run(); });
}

[[nodiscard]] inline bool build_strat(NodeFactoryContext& ctx) {
  auto* runner = ctx.registry.emplace<StratNode>(
      ctx.ports.strat,
      ctx.running,
      ctx.stats,
      ctx.config.order_every_n_ticks,
      trace_idx_for_node(ctx.engine, NodeId::Strat));
  return ctx.engine.add_worker(to_node_id(NodeId::Strat), "strat", [runner]() { runner->run(); });
}

[[nodiscard]] inline bool build_or(NodeFactoryContext& ctx) {
  auto* runner = ctx.registry.emplace<OrNode>(
      ctx.ports.order_router, ctx.running, ctx.stats, trace_idx_for_node(ctx.engine, NodeId::Or));
  return ctx.engine.add_worker(to_node_id(NodeId::Or), "or", [runner]() { runner->run(); });
}

[[nodiscard]] inline bool construct_nodes_and_register(NodeFactoryContext& ctx) {
  struct FactorySpec {
    NodeId node_id;
    NodeFactoryFn factory;
  };

  constexpr std::array<FactorySpec, 3> kFactories {
      FactorySpec {.node_id = NodeId::Md, .factory = &build_md},
      FactorySpec {.node_id = NodeId::Strat, .factory = &build_strat},
      FactorySpec {.node_id = NodeId::Or, .factory = &build_or},
  };

  for (const FactorySpec& spec : kFactories) {
    if (!ctx.engine.core(to_node_id(spec.node_id)).has_value()) {
      continue;
    }
    if (spec.factory == nullptr || !spec.factory(ctx)) {
      return false;
    }
  }
  return true;
}

}  // namespace magus2::mvp::assembly
