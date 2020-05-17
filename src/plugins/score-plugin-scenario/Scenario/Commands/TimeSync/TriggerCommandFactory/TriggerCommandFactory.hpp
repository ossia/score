#pragma once
#include <score/plugins/Interface.hpp>

#include <score_plugin_scenario_export.h>
namespace score
{
class Command;
}
namespace Scenario
{
class TimeSyncModel;
namespace Command
{

class SCORE_PLUGIN_SCENARIO_EXPORT TriggerCommandFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(TriggerCommandFactory, "d6b7385e-b6c4-4cc2-8fc6-1041a43d98fa")
public:
  virtual ~TriggerCommandFactory();
  virtual bool matches(const TimeSyncModel& tn) const = 0;
  virtual score::Command* make_addTriggerCommand(const TimeSyncModel& tn) const = 0;
  virtual score::Command* make_removeTriggerCommand(const TimeSyncModel& tn) const = 0;
};
}
}
