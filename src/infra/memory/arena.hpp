#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

namespace magus2::infra::memory {

class Arena {
public:
  virtual ~Arena() = default;

  virtual void* allocate(std::size_t bytes, std::size_t alignment) noexcept = 0;
  virtual void deallocate(void* ptr, std::size_t bytes, std::size_t alignment) noexcept = 0;
};

class BumpArena final : public Arena {
public:
  explicit BumpArena(std::size_t capacity_bytes)
      : data_(capacity_bytes == 0U ? nullptr : std::make_unique<std::byte[]>(capacity_bytes))
      , capacity_(capacity_bytes) {}

  void* allocate(std::size_t bytes, std::size_t alignment) noexcept override {
    if (bytes == 0U) {
      return nullptr;
    }

    if (alignment == 0U) {
      alignment = alignof(std::max_align_t);
    }

    if (data_ && capacity_ > offset_) {
      void* current = data_.get() + offset_;
      std::size_t space = capacity_ - offset_;
      if (std::align(alignment, bytes, current, space) != nullptr) {
        const auto used = static_cast<std::size_t>(static_cast<std::byte*>(current) - data_.get());
        offset_ = used + bytes;
        return current;
      }
    }

    return nullptr;
  }

  void deallocate(void* ptr, std::size_t bytes, std::size_t alignment) noexcept override {
    (void)bytes;
    (void)alignment;
    if (ptr == nullptr) {
      return;
    }

    if (owns(ptr)) {
      return;
    }
  }

  void reset() noexcept { offset_ = 0; }

  [[nodiscard]] std::size_t used_bytes() const noexcept { return offset_; }
  [[nodiscard]] std::size_t capacity_bytes() const noexcept { return capacity_; }

private:
  [[nodiscard]] bool owns(void* ptr) const noexcept {
    if (!data_) {
      return false;
    }

    const auto* begin = data_.get();
    const auto* end = begin + capacity_;
    const auto* p = static_cast<std::byte*>(ptr);
    return p >= begin && p < end;
  }

  std::unique_ptr<std::byte[]> data_;
  std::size_t capacity_ {0};
  std::size_t offset_ {0};
};

template<typename T>
class ArenaAllocator {
public:
  using value_type = T;

  ArenaAllocator() noexcept = default;
  explicit ArenaAllocator(Arena& arena) noexcept
      : arena_(&arena) {}

  template<typename U>
  ArenaAllocator(const ArenaAllocator<U>& other) noexcept
      : arena_(other.arena_) {}

  [[nodiscard]] T* allocate(std::size_t n) {
    if (arena_ == nullptr || n == 0U) {
      throw std::bad_alloc();
    }

    void* ptr = arena_->allocate(n * sizeof(T), alignof(T));
    if (ptr == nullptr) {
      throw std::bad_alloc();
    }
    return static_cast<T*>(ptr);
  }

  void deallocate(T* ptr, std::size_t n) noexcept {
    if (arena_ == nullptr) {
      return;
    }
    arena_->deallocate(ptr, n * sizeof(T), alignof(T));
  }

  template<typename U>
  [[nodiscard]] bool operator==(const ArenaAllocator<U>& other) const noexcept {
    return arena_ == other.arena_;
  }

  template<typename U>
  [[nodiscard]] bool operator!=(const ArenaAllocator<U>& other) const noexcept {
    return !(*this == other);
  }

private:
  template<typename U>
  friend class ArenaAllocator;

  Arena* arena_ {nullptr};
};

}  // namespace magus2::infra::memory
