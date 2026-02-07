#pragma once

#include <rigtorp/rigtorp.hpp>

namespace magus2::infra::topology {

template<typename T>
struct RxPort {
  rigtorp::SPSCQueue<T>* q {nullptr};

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
  rigtorp::SPSCQueue<T>* q {nullptr};

  [[nodiscard]] bool present() const noexcept { return q != nullptr; }

  bool try_send(const T& msg) noexcept {
    if (q == nullptr) {
      return false;
    }
    return q->try_push(msg);
  }
};

}  // namespace magus2::infra::topology
