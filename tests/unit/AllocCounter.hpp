#pragma once

// Counts the heap allocations, and separately the frees, a thread makes while
// an alloc_counter::scope is alive on it. Every replaceable form of operator new and delete is replaced:
// plain, array, nothrow, aligned and sized. Allocations made by other threads
// (a Qt worker, the logger, Catch2's reporter) are never counted, as the flag
// that enables counting is thread_local.
//
// The replacements are definitions, not inline functions (the standard forbids
// inline replacements): include this header in exactly one translation unit of
// a test executable.

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace alloc_counter
{
inline constinit thread_local bool counting = false;
inline std::atomic<int> count{0};
inline std::atomic<int> free_count{0};

//! Counts the allocations of the current thread from construction to
//! destruction. Starts from zero; scopes do not nest.
struct scope
{
  scope() noexcept
  {
    count.store(0, std::memory_order_relaxed);
    free_count.store(0, std::memory_order_relaxed);
    counting = true;
  }
  ~scope() { counting = false; }
  scope(const scope&) = delete;
  scope& operator=(const scope&) = delete;

  //! Allocations made so far in this scope.
  int allocations() const noexcept { return count.load(std::memory_order_relaxed); }

  //! Frees made so far in this scope, of memory allocated anywhere.
  int frees() const noexcept { return free_count.load(std::memory_order_relaxed); }
};

namespace detail
{
inline void note() noexcept
{
  if(counting)
    count.fetch_add(1, std::memory_order_relaxed);
}

inline void* allocate(std::size_t n) noexcept
{
  note();
  return std::malloc(n ? n : 1);
}

inline void* allocate(std::size_t n, std::align_val_t al) noexcept
{
  note();
  auto a = static_cast<std::size_t>(al);
  if(a < sizeof(void*))
    a = sizeof(void*);
  // aligned_alloc wants a size that is a multiple of the alignment.
  n = ((n ? n : 1) + a - 1) / a * a;
#if defined(_WIN32)
  return _aligned_malloc(n, a);
#else
  return std::aligned_alloc(a, n);
#endif
}

inline void note_free(void* p) noexcept
{
  if(counting && p)
    free_count.fetch_add(1, std::memory_order_relaxed);
}

inline void release(void* p) noexcept
{
  note_free(p);
  std::free(p);
}

inline void release_aligned(void* p) noexcept
{
  note_free(p);
#if defined(_WIN32)
  _aligned_free(p);
#else
  std::free(p);
#endif
}
}
}

// clang-format off
void* operator new(std::size_t n)
{
  if(void* p = alloc_counter::detail::allocate(n)) return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t n)
{
  if(void* p = alloc_counter::detail::allocate(n)) return p;
  throw std::bad_alloc{};
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept
{ return alloc_counter::detail::allocate(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept
{ return alloc_counter::detail::allocate(n); }

void* operator new(std::size_t n, std::align_val_t al)
{
  if(void* p = alloc_counter::detail::allocate(n, al)) return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t n, std::align_val_t al)
{
  if(void* p = alloc_counter::detail::allocate(n, al)) return p;
  throw std::bad_alloc{};
}
void* operator new(std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept
{ return alloc_counter::detail::allocate(n, al); }
void* operator new[](std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept
{ return alloc_counter::detail::allocate(n, al); }

void operator delete(void* p) noexcept { alloc_counter::detail::release(p); }
void operator delete[](void* p) noexcept { alloc_counter::detail::release(p); }
void operator delete(void* p, std::size_t) noexcept { alloc_counter::detail::release(p); }
void operator delete[](void* p, std::size_t) noexcept { alloc_counter::detail::release(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { alloc_counter::detail::release(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { alloc_counter::detail::release(p); }

void operator delete(void* p, std::align_val_t) noexcept
{ alloc_counter::detail::release_aligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept
{ alloc_counter::detail::release_aligned(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept
{ alloc_counter::detail::release_aligned(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept
{ alloc_counter::detail::release_aligned(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept
{ alloc_counter::detail::release_aligned(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept
{ alloc_counter::detail::release_aligned(p); }
// clang-format on
