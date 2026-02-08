#include "mvp/runtime.hpp"

#include "mvp/nodes.hpp"
#include "mvp/wiring.hpp"  // MVP assembly layer (binds generic infra to concrete node roles)

#include <infra/memory/arena.hpp>

#include <atomic>
#include <memory>

namespace magus2::mvp {

struct Runtime::Impl {
  explicit Impl(infra::topology::Topology topology, MvpConfig cfg)
      : config(cfg)
      , queue_arena(cfg.queue_arena_bytes)
      , engine(std::move(topology), queue_arena) {}

  MvpConfig config;
  infra::memory::BumpArena queue_arena;
  assembly::Engine engine;

  RuntimeCounters stats;
  std::atomic<bool> running {false};

  assembly::AppPortBundle ports;
  assembly::NodeRegistry nodes;

  bool built {false};
  bool started {false};
  std::string error;

  bool build() {
    if (built) {
      return true;
    }

    if (!engine.build()) {
      error = engine.last_error();
      return false;
    }

    if (!assembly::bind_ports(engine, ports)) {
      error = engine.last_error();
      return false;
    }

    assembly::NodeFactoryContext factory_ctx {
        .engine = engine,
        .running = running,
        .stats = stats,
        .config = config,
        .ports = ports,
        .registry = nodes,
    };

    if (!assembly::construct_nodes_and_register(factory_ctx)) {
      error = engine.last_error();
      return false;
    }

    built = true;
    return true;
  }

  bool start() {
    if (!built && !build()) {
      return false;
    }
    if (started) {
      return true;
    }

    running.store(true, std::memory_order_release);

    if (!engine.start()) {
      running.store(false, std::memory_order_release);
      error = engine.last_error();
      return false;
    }

    started = true;
    return true;
  }

  void stop() {
    running.store(false, std::memory_order_release);
  }

  void join() {
    engine.join();
    started = false;
  }

  [[nodiscard]] bool try_push_tick(const TickEnvelope& tick) noexcept {
    return ports.ingress.tick_tx.try_send(tick);
  }

  [[nodiscard]] const RuntimeCounters& counters() const noexcept {
    return stats;
  }
};

Runtime::Runtime(infra::topology::Topology topology, MvpConfig config)
    : impl_(std::make_unique<Impl>(std::move(topology), config)) {}

Runtime::~Runtime() {
  stop();
  join();
}

bool Runtime::build() {
  return impl_->build();
}

bool Runtime::start() {
  return impl_->start();
}

void Runtime::stop() {
  impl_->stop();
}

void Runtime::join() {
  impl_->join();
}

bool Runtime::try_push_tick(const TickEnvelope& tick) noexcept {
  return impl_->try_push_tick(tick);
}

const RuntimeCounters& Runtime::counters() const noexcept {
  return impl_->counters();
}

const std::string& Runtime::last_error() const noexcept {
  return impl_->error;
}

}  // namespace magus2::mvp
