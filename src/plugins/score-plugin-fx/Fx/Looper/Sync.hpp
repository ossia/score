#pragma once
#include <halp/polyfill.hpp>
#include <halp/shared_instance.hpp>
#include <halp/static_string.hpp>

#include <avnd/common/enum_reflection.hpp>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>

namespace Nodes::Looper
{
//! Loopers that play together: the first one to record sets the loop of the
//! group, the others' loops are a whole number of it, or a half, a quarter or
//! an eighth of it, and go round in phase with it.
enum class SyncGroup
{
  None,
  A,
  B,
  C,
  D
};

//! An enumeration shown as a combobox: compact where the choices are many.
template <halp::static_string Name, typename Enum, Enum Init>
struct enum_combobox
{
  static clang_buggy_consteval auto name() { return std::string_view{Name.value}; }
  enum widget
  {
    combobox
  };
  struct range
  {
    decltype(avnd::enum_names<Enum>()) values = avnd::enum_names<Enum>();
    Enum init = Init;
  };
  Enum value = Init;
  operator Enum() const noexcept { return value; }
  auto& operator=(Enum t) noexcept
  {
    value = t;
    return *this;
  }
};

struct sync_registry
{
  struct group
  {
    std::atomic<int64_t> length{0};
    //! Engine frame at which the group's loop starts.
    std::atomic<int64_t> origin{0};
    std::atomic<const void*> leader{nullptr};
  };
  std::array<group, 4> groups;
};

/**
 * A looper's side of its sync group. Lock-free: the registry is shared
 * between loopers through atomics; only getting it, at construction, takes a
 * lock, on the main thread.
 */
struct loop_sync
{
  std::shared_ptr<sync_registry> registry = halp::shared_instance<sync_registry>();
  SyncGroup group{SyncGroup::None};

  ~loop_sync() { release(); }

  sync_registry::group* current() const noexcept
  {
    if(group == SyncGroup::None || !registry)
      return nullptr;
    return &registry->groups[int(group) - 1];
  }

  struct result
  {
    int64_t length;
    //! Where playback carries on in the loop.
    int64_t position;
  };

  //! A recording of `length` frames closed at engine frame `now`, where
  //! the loop plays on from `position`: 0 after a recording that ended on time,
  //! more after one that ended late. Leads the group if nobody does; else
  //! follows it.
  result close(int64_t length, int64_t now, int64_t position) noexcept
  {
    auto* g = current();
    if(!g || length <= 0)
      return {length, position};

    const void* expected = nullptr;
    if(g->leader.compare_exchange_strong(expected, this) || expected == this)
    {
      g->length.store(length);
      g->origin.store(now - position % length);
      return {length, position % length};
    }

    const int64_t master = g->length.load();
    if(master <= 0)
      return {length, position};

    const int64_t synced = fit(length, master);
    int64_t pos = (now - g->origin.load()) % synced;
    if(pos < 0)
      pos += synced;
    return {synced, pos};
  }

  //! The loop of a group member is the master's times a whole number, or a
  //! half, a quarter or an eighth of it, whichever is nearest.
  static int64_t fit(int64_t length, int64_t master) noexcept
  {
    const double ratio = double(length) / double(master);
    if(ratio >= 0.75)
      return master * std::max<int64_t>(1, std::llround(ratio));
    int64_t best = master / 2;
    for(int64_t d : {2, 4, 8})
      if(std::abs(double(master) / d - double(length)) < std::abs(double(best) - double(length)))
        best = master / d;
    return std::max<int64_t>(1, best);
  }

  //! The looper was cleared or goes away: if it led, the group is free.
  void release() noexcept
  {
    auto* g = current();
    if(!g)
      return;
    const void* self = this;
    if(g->leader.compare_exchange_strong(self, nullptr))
      g->length.store(0);
  }

  void set_group(SyncGroup g) noexcept
  {
    if(g == group)
      return;
    release();
    group = g;
  }
};
}
