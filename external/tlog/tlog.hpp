// tlog - a tiny trace/logging helper over fmtlog
//
// Features
// - Header-only, POD carriers for SPSC headers
// - TLS context (trace/span/correlation) with zero-allocation scopes
// - Constant-time, fixed-size snapshot injected into fmtlog calls
// - No dynamic strings, no maps, no timestamps (fmtlog handles those)
//
// Dependencies: fmt, fmtlog
// Requires C++20 for consteval hashing (TLOG_KEY).
//
// Usage (typical)
//   tlog::init_thread(0);
//
//   void on_new_work(u64 order_id) {
//     TINGRESS("order_id", order_id);
//     TSPAN();
//     TLOGI("event=ingress");
//
//     Msg m; TSEND(m); q.push(m);
//   }
//
//   void on_msg(const Msg& m) {
//     TADOPT(m.ctx);
//     TSPAN();
//     TLOGI("event=stage.price");
//   }
//
// Semantics
// - carrier stores the *current span* and its parent (from the sender)
// - adopt() restores that current span and its parent
// - TSPAN() creates a child span (parent = previously current span)
//
#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>
#include <cassert>
#include <string_view>

#ifndef TLOG_NOFMTLOG
  #include <fmt/core.h>
  #include <fmtlog/fmtlog.h>
#endif
#if defined(_MSC_VER) && defined(TLOG_TRACE_SOURCE_RDTSC)
  #include <intrin.h>
#endif

#if defined(_MSC_VER)
  #define TLOG_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
  #define TLOG_FORCE_INLINE __attribute__((always_inline)) inline
#else
  #define TLOG_FORCE_INLINE inline
#endif

namespace tlog::inline v0_1_0 {
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

struct trace_id { u64 hi = 0, lo = 0; };
struct span_id  { u64 v = 0; };

// W3C/OTel-compatible primitives: trace_id (16B), span_id (8B), flags (sampling bit etc)
struct span_ctx { trace_id tid; span_id sid; u8 flags = 0; };

// compile-time key hash (so "rfq_id" isn't stored per record; avoids string handling)
consteval u32 h32(const char* s, u32 h = 2166136261u) {
  return *s ? h32(s + 1, (h ^ (u8)*s) * 16777619u) : h;
}
#define TLOG_KEY(lit) (::tlog::h32(lit))

// correlation key: one pair, fixed-size, always-on
struct corr { u32 k = 0; u64 v = 0; };

// what you propagate in SPSC messages (current span, parent span, trace, corr)
struct alignas(8) carrier { span_ctx c{}; u64 parent = 0; corr x{}; };

// TLS state
struct tls_context { span_ctx c{}; corr x{}; u64 parent = 0; };
struct tls_state { tls_context ctx{}; u64 span_seq = 0; u16 tidx = 0xffff; };
inline thread_local tls_state tls;

// hooks for app to supply trace ids (you likely already have a fast RNG/idgen)
using trace_source_fn = trace_id(*)();
inline trace_source_fn trace_source = nullptr;
inline void set_trace_source(trace_source_fn f) noexcept { trace_source = f; }

inline void init_thread(u16 tidx) noexcept { tls.tidx = tidx; }
inline span_id new_span() noexcept {
#ifndef TLOG_ASSUME_TLS_INIT
  assert(tls.tidx != 0xffff && "tlog::init_thread() must be called per thread");
#endif
  u64 seq = ++tls.span_seq;
  if (seq == 0) seq = ++tls.span_seq; // avoid 0 on wrap
  return { (u64(tls.tidx) << 48) | (seq & ((1ull << 48) - 1)) };
}

// update correlation key without starting a new trace
inline void set_corr(u32 corr_key, u64 corr_val) noexcept {
  tls.ctx.x = { corr_key, corr_val };
}

// update trace flags without starting a new trace
inline void set_flags(u8 flags) noexcept { tls.ctx.c.flags = flags; }
inline void or_flags(u8 flags) noexcept { tls.ctx.c.flags |= flags; }

// reset trace/span/correlation context (keeps tidx/span_seq)
inline void reset() noexcept { tls.ctx = {}; }

// zeroed carrier for safe initialization in pooled messages
inline carrier carrier_zero() noexcept { return carrier{}; }

// start a new trace at ingress (generic “work item enters process”)
inline void ingress(u32 corr_key, u64 corr_val, u8 flags = 0) noexcept {
#ifndef TLOG_ASSUME_TLS_INIT
  assert(tls.tidx != 0xffff && "tlog::init_thread() must be called per thread");
#endif
#if defined(TLOG_TRACE_SOURCE_REQUIRED)
  assert(trace_source && "tlog::set_trace_source() must be configured");
  tls.ctx.c.tid = trace_source();
#elif defined(TLOG_TRACE_SOURCE_RDTSC)
  if (trace_source) {
    tls.ctx.c.tid = trace_source();
  } else {
  #if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
    #if defined(_MSC_VER)
      const u64 t = __rdtsc();
    #else
      const u64 t = __builtin_ia32_rdtsc();
    #endif
    tls.ctx.c.tid = { (u64(tls.tidx) << 48) ^ (t >> 16), t };
  #else
    const u64 t = (u64(tls.tidx) << 48) | (tls.span_seq & ((1ull << 48) - 1));
    tls.ctx.c.tid = { t ^ 0x9e3779b97f4a7c15ull, t };
  #endif
  }
#else
  tls.ctx.c.tid = trace_source ? trace_source() : trace_id{};
#endif
  tls.ctx.c.flags = flags;
  tls.ctx.x = { corr_key, corr_val };
  tls.ctx.parent = 0;
  tls.ctx.c.sid = new_span();
}

// start a new trace with placeholder correlation (fill later with set_corr)
inline void ingress_pending(u32 corr_key, u8 flags = 0) noexcept {
  ingress(corr_key, 0, flags);
}

// adopt propagated context (top of consumer handler)
inline void adopt(const carrier& in) noexcept {
  tls.ctx.c = in.c;
  tls.ctx.parent = in.parent;
  tls.ctx.x = in.x;
}

// capture carrier for downstream hop (call before enqueue)
inline carrier carry() noexcept { return { tls.ctx.c, tls.ctx.parent, tls.ctx.x }; }

// RAII scopes (lexical, zero alloc)
struct scope_adopt {
  tls_context prev;
  explicit scope_adopt(const carrier& in) noexcept : prev(tls.ctx) { adopt(in); }
  ~scope_adopt() noexcept { tls.ctx = prev; }
};

struct scope_span {
  u64 prev_sid;
  u64 prev_parent;
  scope_span() noexcept : prev_sid(tls.ctx.c.sid.v), prev_parent(tls.ctx.parent) {
    tls.ctx.parent = tls.ctx.c.sid.v;
    tls.ctx.c.sid = new_span();
  }
  ~scope_span() noexcept {
    tls.ctx.c.sid.v = prev_sid;
    tls.ctx.parent = prev_parent;
  }
};

// snapshot injected into each fmtlog call (POD copy, formatting later)
struct alignas(8) snap { span_ctx c; u64 parent; corr x; u16 tidx; };
TLOG_FORCE_INLINE snap snap_now() noexcept {
  return { tls.ctx.c, tls.ctx.parent, tls.ctx.x, tls.tidx };
}

} // namespace tlog

// logging macros (preserve fmtlog callsite optimizations)
#ifndef TLOG_NOFMTLOG
template<> struct fmt::formatter<tlog::snap> : fmt::formatter<fmt::string_view> {
  template<class Ctx>
  auto format(const tlog::snap& s, Ctx& ctx) const {
    return fmt::format_to(ctx.out(),
      "tid={} trace={:016x}{:016x} span={:016x} parent={:016x} k{:08x}={:016x} f={:02x}",
      s.tidx, s.c.tid.hi, s.c.tid.lo, s.c.sid.v, s.parent, s.x.k, s.x.v, s.c.flags);
  }
};

namespace tlog {
// Intentionally empty: TLOG* macros call fmtlog macros directly.
} // namespace tlog

#define TLOGD(fmt_, ...) logd("[{}] " fmt_, ::tlog::snap_now() __VA_OPT__(,) __VA_ARGS__)
#define TLOGI(fmt_, ...) logi("[{}] " fmt_, ::tlog::snap_now() __VA_OPT__(,) __VA_ARGS__)
#define TLOGW(fmt_, ...) logw("[{}] " fmt_, ::tlog::snap_now() __VA_OPT__(,) __VA_ARGS__)
#define TLOGE(fmt_, ...) loge("[{}] " fmt_, ::tlog::snap_now() __VA_OPT__(,) __VA_ARGS__)
#else
namespace tlog {
template <class... Args>
TLOG_FORCE_INLINE void emitd(std::string_view, Args&&...) {}
template <class... Args>
TLOG_FORCE_INLINE void emiti(std::string_view, Args&&...) {}
template <class... Args>
TLOG_FORCE_INLINE void emitw(std::string_view, Args&&...) {}
template <class... Args>
TLOG_FORCE_INLINE void emite(std::string_view, Args&&...) {}
} // namespace tlog

#define TLOGD(fmt_, ...) ((void)0)
#define TLOGI(fmt_, ...) ((void)0)
#define TLOGW(fmt_, ...) ((void)0)
#define TLOGE(fmt_, ...) ((void)0)
#endif

// unique-scope helpers
#define TLOG_CAT_(a,b) a##b
#define TLOG_CAT(a,b)  TLOG_CAT_(a,b)
#define TLOG_UNIQ(p)   TLOG_CAT(p, __COUNTER__)

// sugar
#define TINGRESS(key_lit, val_u64) ::tlog::ingress(TLOG_KEY(key_lit), (val_u64))
#define TINGRESS_PENDING(key_lit) ::tlog::ingress_pending(TLOG_KEY(key_lit))
#define TADOPT(car) ::tlog::scope_adopt TLOG_UNIQ(_adopt_){(car)}
#define TSPAN()     ::tlog::scope_span  TLOG_UNIQ(_span_){}
#define TSEND(msg)  (msg).ctx = ::tlog::carry()

#ifndef NTEST
namespace tlog::test {
consteval u32 hash_order_id() { return h32("order_id"); }
static_assert(hash_order_id() == TLOG_KEY("order_id"));
} // namespace tlog::test

static_assert(std::is_trivially_copyable_v<tlog::trace_id>);
static_assert(std::is_trivially_copyable_v<tlog::span_id>);
static_assert(std::is_trivially_copyable_v<tlog::span_ctx>);
static_assert(std::is_trivially_copyable_v<tlog::corr>);
static_assert(std::is_trivially_copyable_v<tlog::carrier>);
static_assert(std::is_trivially_copyable_v<tlog::snap>);
static_assert(std::is_standard_layout_v<tlog::carrier>);
static_assert(std::is_standard_layout_v<tlog::snap>);
static_assert(alignof(tlog::carrier) >= alignof(tlog::u64));
static_assert(alignof(tlog::snap) >= alignof(tlog::u64));
static_assert(sizeof(tlog::trace_id) == 16);
static_assert(sizeof(tlog::span_id) == 8);
static_assert(sizeof(tlog::span_ctx) >= 24);
static_assert(sizeof(tlog::carrier) == sizeof(tlog::span_ctx) + sizeof(tlog::corr) + sizeof(tlog::u64));
#endif

#if defined(TLOG_SELFTEST)
#include <cassert>
namespace tlog::test {
inline void selftest() {
  init_thread(1);
  ingress(TLOG_KEY("order_id"), 42);
  const auto s0 = snap_now();
  carrier c = carry();
  {
    scope_adopt sa{c};
    scope_span sp;
    const auto s1 = snap_now();
    assert(s1.parent == c.c.sid.v);
    assert(s1.c.tid.hi == s0.c.tid.hi);
    assert(s1.c.tid.lo == s0.c.tid.lo);
  }
}
} // namespace tlog::test
#endif
