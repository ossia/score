#pragma once
#include <Process/Process.hpp>
#include <iscore_plugin_scenario_export.h>
namespace Scenario
{
struct ISCORE_PLUGIN_SCENARIO_EXPORT Slot
{
  enum RackView { SmallView, FullView };

  std::vector<Id<Process::ProcessModel>> processes;
  OptionalId<Process::ProcessModel> frontProcess;
  qreal height{200};
  bool focus{};
};

using Rack = std::vector<Slot>;
struct FullSlot {
  Id<Process::ProcessModel> process;
};
using FullRack = std::vector<FullSlot>;
class ConstraintModel;

struct ISCORE_PLUGIN_SCENARIO_EXPORT SlotPath
{
  Path<ConstraintModel> constraint;
  int index{};
  Slot::RackView full_view{};

  const Slot& find() const;
  const Slot* try_find() const;
};

struct ISCORE_PLUGIN_SCENARIO_EXPORT SlotId
{
  SlotId() = default;
  SlotId(const SlotId&) = default;
  SlotId& operator=(const SlotId&) = default;

  SlotId(std::size_t p, Slot::RackView f)
    : index{(int)p}
    , view{f} {}
  SlotId(int p, Slot::RackView f)
    : index{p}
    , view{f} {}

  SlotId(const SlotPath& p)
    : index{p.index}
    , view{p.full_view}
  {

  }

  int index{};
  Slot::RackView view{};

  bool fullView() const { return view == Slot::FullView; }
  bool smallView() const { return view == Slot::SmallView; }
};


}
