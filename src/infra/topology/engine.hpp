#pragma once

#include "infra/memory/arena.hpp"
#include "infra/topology/ports.hpp"
#include "infra/topology/runtime.hpp"
#include "infra/topology/spec.hpp"

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <new>
#include <optional>
#include <rigtorp/rigtorp.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace magus2::infra::topology {

template<auto ContractValueV, typename EnvelopeT>
struct ContractBinding {
  static constexpr auto contract_value = ContractValueV;
  using envelope_type = EnvelopeT;
};

struct QueueBase {
  virtual ~QueueBase() = default;
};

template<typename T>
struct QueueStorage final : QueueBase {
  QueueStorage(std::size_t depth, memory::Arena& arena)
      : queue(depth, memory::ArenaAllocator<T> {arena}) {}

  SpscQueue<T> queue;
};

template<typename... Bindings>
class QueueStore {
public:
  ~QueueStore() {
    clear();
  }

  bool build(const Topology& topology, memory::Arena& arena, std::string& error) {
    clear();
    arena_ = &arena;
    capacity_ = topology.edges.size();
    if (capacity_ > 0U) {
      void* mem = arena.allocate(sizeof(QueueEntry) * capacity_, alignof(QueueEntry));
      if (mem == nullptr) {
        error = "queue entry allocation failed";
        clear();
        return false;
      }
      entries_ = static_cast<QueueEntry*>(mem);
    }

    for (const EdgeSpec& edge : topology.edges) {
      if (edge.depth < 2U) {
        error = "edge depth must be >= 2";
        clear();
        return false;
      }

      QueueEntry queue = make_queue(edge.contract, edge.depth, arena);
      if (queue.base == nullptr) {
        if (!supports_contract(edge.contract)) {
          error = "unknown contract id=" + std::to_string(edge.contract);
        } else {
          error = "queue allocation failed for contract id=" + std::to_string(edge.contract);
        }
        clear();
        return false;
      }

      entries_[size_++] = queue;
    }

    return true;
  }

  void clear() noexcept {
    if (arena_ != nullptr && entries_ != nullptr) {
      for (std::size_t i = size_; i > 0U; --i) {
        QueueEntry& entry = entries_[i - 1U];
        if (entry.base != nullptr && entry.destroy != nullptr) {
          entry.destroy(*arena_, entry.base);
        }
      }
      arena_->deallocate(entries_, sizeof(QueueEntry) * capacity_, alignof(QueueEntry));
    }

    entries_ = nullptr;
    size_ = 0;
    capacity_ = 0;
  }

  template<typename T>
  [[nodiscard]] SpscQueue<T>* queue_as(std::size_t edge_index) noexcept {
    if (entries_ == nullptr || edge_index >= size_) {
      return nullptr;
    }

    auto* typed = dynamic_cast<QueueStorage<T>*>(entries_[edge_index].base);
    return typed == nullptr ? nullptr : &typed->queue;
  }

private:
  struct QueueEntry {
    QueueBase* base {nullptr};
    void (*destroy)(memory::Arena&, QueueBase*) {nullptr};
  };

  template<typename QueueT>
  static void destroy_queue(memory::Arena& arena, QueueBase* base) noexcept {
    auto* typed = static_cast<QueueT*>(base);
    typed->~QueueT();
    arena.deallocate(typed, sizeof(QueueT), alignof(QueueT));
  }

  template<typename QueueT>
  [[nodiscard]] static QueueEntry allocate_queue(memory::Arena& arena, std::size_t depth) {
    void* mem = arena.allocate(sizeof(QueueT), alignof(QueueT));
    if (mem == nullptr) {
      return {};
    }

    QueueT* queue = nullptr;
    try {
      queue = new(mem) QueueT(depth, arena);
    } catch (...) {
      arena.deallocate(mem, sizeof(QueueT), alignof(QueueT));
      return {};
    }

    return QueueEntry {.base = queue, .destroy = &destroy_queue<QueueT>};
  }

  template<typename Binding>
  [[nodiscard]] static QueueEntry try_make(ContractId contract_id, std::size_t depth, memory::Arena& arena) {
    if (contract_id != static_cast<ContractId>(Binding::contract_value)) {
      return {};
    }

    using QueueT = QueueStorage<typename Binding::envelope_type>;
    return allocate_queue<QueueT>(arena, depth);
  }

  [[nodiscard]] static bool supports_contract(ContractId contract_id) noexcept {
    return ((contract_id == static_cast<ContractId>(Bindings::contract_value)) || ...);
  }

  [[nodiscard]] QueueEntry make_queue(ContractId contract_id, std::size_t depth, memory::Arena& arena) {
    QueueEntry out {};
    (void)std::initializer_list<int> {
        (out.base ? 0 : (out = try_make<Bindings>(contract_id, depth, arena), 0))...,
    };
    return out;
  }

  QueueEntry* entries_ {nullptr};
  std::size_t size_ {0};
  std::size_t capacity_ {0};
  memory::Arena* arena_ {nullptr};
};

template<typename QueueStoreT>
class Engine {
public:
  Engine(Topology topology, memory::Arena& arena)
      : topology_(std::move(topology))
      , arena_(arena) {}

  bool build() {
    if (built_) {
      return true;
    }

    if (!validate_ports(topology_, error_)) {
      return false;
    }

    if (!queues_.build(topology_, arena_, error_)) {
      return false;
    }

    built_ = true;
    return true;
  }

  template<typename T, typename ContractT>
  bool bind_rx(NodeId node_id, std::string_view port_name, ContractT contract, Inbox<T>& port) {
    return bind_port<T>(node_id, port_name, static_cast<ContractId>(contract), Direction::Rx, port);
  }

  template<typename T, typename ContractT>
  bool bind_tx(NodeId node_id, std::string_view port_name, ContractT contract, Outbox<T>& port) {
    return bind_port<T>(node_id, port_name, static_cast<ContractId>(contract), Direction::Tx, port);
  }

  bool add_worker(NodeId node_id, std::string_view worker_name, std::function<void()> run) {
    if (!run) {
      error_ = "worker run function is empty for node=" + std::to_string(node_id);
      return false;
    }

    workers_.push_back(WorkerPlan {.node_id = node_id, .name = std::string(worker_name), .run = std::move(run)});
    return true;
  }

  bool start() {
    if (!built_ && !build()) {
      return false;
    }

    if (started_) {
      return true;
    }

    std::vector<WorkerSpec> workers;
    workers.reserve(workers_.size());

    for (const WorkerPlan& plan : workers_) {
      const auto core = find_core(topology_, plan.node_id);
      if (!core) {
        error_ = "missing core mapping for node_id=" + std::to_string(plan.node_id);
        return false;
      }

      workers.push_back(WorkerSpec {.name = plan.name, .core = *core, .run = plan.run});
    }

    if (!thread_runtime_.launch(workers)) {
      error_ = thread_runtime_.last_error();
      return false;
    }

    started_ = true;
    return true;
  }

  void join() {
    thread_runtime_.join();
    started_ = false;
  }

  [[nodiscard]] const std::string& last_error() const noexcept { return error_; }

  [[nodiscard]] std::optional<std::uint32_t> core(NodeId node_id) const noexcept {
    return find_core(topology_, node_id);
  }

private:
  struct WorkerPlan {
    NodeId node_id {0};
    std::string name;
    std::function<void()> run;
  };

  template<typename T, typename PortT>
  bool bind_port(NodeId node_id, std::string_view port_name, ContractId contract, Direction dir, PortT& port) {
    const auto edge_index = find_edge_index(topology_, node_id, port_name, dir, contract);
    if (!edge_index) {
      error_ = "missing port binding node=" + std::to_string(node_id) + " port=" + std::string(port_name);
      return false;
    }

    port.q = queues_.template queue_as<T>(*edge_index);
    if (!port.present()) {
      error_ = "queue type mismatch node=" + std::to_string(node_id) + " port=" + std::string(port_name);
      return false;
    }

    return true;
  }

  Topology topology_;
  memory::Arena& arena_;
  QueueStoreT queues_;
  ThreadRuntime thread_runtime_;
  std::vector<WorkerPlan> workers_;
  std::string error_;
  bool built_ {false};
  bool started_ {false};
};

}  // namespace magus2::infra::topology
