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
class ConstraintModel;

struct ISCORE_PLUGIN_SCENARIO_EXPORT SlotPath
{
  Path<ConstraintModel> constraint;
  int slot_position{};
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
    , full_view{f} {}
  SlotId(int p, Slot::RackView f)
    : index{p}
    , full_view{f} {}

  SlotId(const SlotPath& p)
    : index{p.slot_position}
    , full_view{p.full_view}
  {

  }

  int index{};
  Slot::RackView full_view{};
};


}
