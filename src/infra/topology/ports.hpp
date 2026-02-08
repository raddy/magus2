#pragma once

#include "infra/memory/arena.hpp"

#include <rigtorp/rigtorp.hpp>

namespace magus2::infra::topology {

template<typename T>
using SpscQueue = rigtorp::SPSCQueue<T, memory::ArenaAllocator<T>>;

template<typename T>
struct RxPort {
  SpscQueue<T>* q {nullptr};

  [[nodiscard]] bool present() const noexcept { return q != nullptr; }

  bool try_recv(T& out) noexcept {
    if (q == nullptr) {
      return false;
    }
    T* front = q->front();
    if (front == nullptr) {
      return false;
    }
    out = *front;
    q->pop();
    return true;
  }
};

template<typename T>
struct TxPort {
  SpscQueue<T>* q {nullptr};

  [[nodiscard]] bool present() const noexcept { return q != nullptr; }

  bool try_send(const T& msg) noexcept {
    if (q == nullptr) {
      return false;
    }
    return q->try_push(msg);
  }
};

template<typename T>
using Inbox = RxPort<T>;

template<typename T>
using Outbox = TxPort<T>;

}  // namespace magus2::infra::topology
