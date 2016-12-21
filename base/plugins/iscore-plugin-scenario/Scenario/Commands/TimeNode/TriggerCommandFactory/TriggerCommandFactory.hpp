#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore_plugin_scenario_export.h>
namespace iscore
{
class Command;
}
namespace Scenario
{
class TimeNodeModel;
namespace Command
{

class ISCORE_PLUGIN_SCENARIO_EXPORT TriggerCommandFactory
    : public iscore::Interface<TriggerCommandFactory>
{
  ISCORE_INTERFACE("d6b7385e-b6c4-4cc2-8fc6-1041a43d98fa")
public:
  virtual ~TriggerCommandFactory();
  virtual bool matches(const TimeNodeModel& tn) const = 0;
  virtual iscore::Command*
  make_addTriggerCommand(const TimeNodeModel& tn) const = 0;
  virtual iscore::Command*
  make_removeTriggerCommand(const TimeNodeModel& tn) const = 0;
};
}
}
