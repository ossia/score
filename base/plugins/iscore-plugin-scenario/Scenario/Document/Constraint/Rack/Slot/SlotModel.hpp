#pragma once
#include <Process/Process.hpp>
#include <iscore_plugin_scenario_export.h>
namespace Scenario
{
struct ISCORE_PLUGIN_SCENARIO_EXPORT Slot
{
  std::vector<Id<Process::ProcessModel>> processes;
  OptionalId<Process::ProcessModel> frontProcess;
  qreal height{200};
  bool focus{};
};

using Rack = std::vector<Slot>;
class ConstraintModel;

struct ISCORE_PLUGIN_SCENARIO_EXPORT SlotIdentifier
{
  Path<ConstraintModel> constraint;
  int slot_position{};
  bool full_view{};

  const Slot& find() const;
  const Slot* try_find() const;
};


}
