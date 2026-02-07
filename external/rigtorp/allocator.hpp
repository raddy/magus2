#pragma once
#include <new>

#include <sys/mman.h>

#ifdef __linux__
#  define RIG_MAP_FLAGS (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB)
#else
#  define RIG_MAP_FLAGS (MAP_PRIVATE | MAP_ANONYMOUS)
#endif

namespace rigtorp
{

template<typename T>
struct Allocator
{
  using value_type = T;

  struct AllocationResult
  {
    T*     ptr;
    size_t count;
  };

  size_t roundup(size_t n) { return (((n - 1) >> 21) + 1) << 21; }

  AllocationResult allocate_at_least(size_t n)
  {
    size_t count = roundup(sizeof(T) * n);
    auto   p = static_cast<T*>(mmap(nullptr, count, PROT_READ | PROT_WRITE, RIG_MAP_FLAGS, -1, 0));
    if (p == MAP_FAILED) {
      throw std::bad_alloc();
    }
    return {p, count / sizeof(T)};
  }

  void deallocate(T* p, size_t n) { munmap(p, roundup(sizeof(T) * n)); }
};

}  // namespace rigtorp