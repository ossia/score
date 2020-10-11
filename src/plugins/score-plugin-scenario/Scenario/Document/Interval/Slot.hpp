#pragma once
#include <Process/Process.hpp>

#include <score_plugin_scenario_export.h>
namespace Scenario
{
struct SCORE_PLUGIN_SCENARIO_EXPORT Slot
{
  enum RackView : bool
  {
    SmallView,
    FullView
  };

  Slot() = default;
  Slot(const Slot&) = default;
  Slot(Slot&&) = default;
  Slot& operator=(const Slot&) = default;
  Slot& operator=(Slot&&) = default;

  Slot(std::vector<Id<Process::ProcessModel>> p) : processes{std::move(p)} { }
  Slot(std::vector<Id<Process::ProcessModel>> p, Id<Process::ProcessModel> fp)
      : processes{std::move(p)}, frontProcess{std::move(fp)}
  {
  }
  Slot(std::vector<Id<Process::ProcessModel>> p, Id<Process::ProcessModel> fp, qreal h)
      : processes{std::move(p)}, frontProcess{std::move(fp)}, height{h}
  {
  }

  std::vector<Id<Process::ProcessModel>> processes;
  OptionalId<Process::ProcessModel> frontProcess;
  qreal height{200};
  bool focus{};
  bool nodal{};
};

using Rack = std::vector<Slot>;
struct FullSlot
{
  Id<Process::ProcessModel> process;
  bool nodal{};
};
using FullRack = std::vector<FullSlot>;
class IntervalModel;

struct SCORE_PLUGIN_SCENARIO_EXPORT SlotPath
{
  SlotPath() = default;
  SlotPath(const SlotPath&) = default;
  SlotPath(SlotPath&&) = default;
  SlotPath& operator=(const SlotPath&) = default;
  SlotPath& operator=(SlotPath&&) = default;

  SlotPath(Path<IntervalModel> p) : interval{std::move(p)} { }
  SlotPath(Path<IntervalModel> p, int idx) : interval{std::move(p)}, index{idx} { }
  SlotPath(Path<IntervalModel> p, int idx, Slot::RackView v)
      : interval{std::move(p)}, index{idx}, full_view{v}
  {
  }

  Path<IntervalModel> interval;
  int index{};
  Slot::RackView full_view{};

  const Slot& find(const score::DocumentContext&) const;
  const Slot* try_find(const score::DocumentContext&) const;
};

struct SCORE_PLUGIN_SCENARIO_EXPORT SlotId
{
  SlotId() = default;
  SlotId(const SlotId&) = default;
  SlotId& operator=(const SlotId&) = default;

  SlotId(std::size_t p, Slot::RackView f) : index{(int)p}, view{f} { }
  SlotId(int p, Slot::RackView f) : index{p}, view{f} { }

  SlotId(const SlotPath& p) : index{p.index}, view{p.full_view} { }

  int index{};
  Slot::RackView view{};

  bool fullView() const { return view == Slot::FullView; }
  bool smallView() const { return view == Slot::SmallView; }
};
}

W_REGISTER_ARGTYPE(Scenario::Slot)
W_REGISTER_ARGTYPE(Scenario::Rack)
W_REGISTER_ARGTYPE(Scenario::Slot::RackView)
W_REGISTER_ARGTYPE(Scenario::FullSlot)
W_REGISTER_ARGTYPE(Scenario::SlotPath)
W_REGISTER_ARGTYPE(Scenario::SlotId)
