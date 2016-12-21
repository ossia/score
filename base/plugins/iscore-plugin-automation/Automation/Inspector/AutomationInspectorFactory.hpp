#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/Inspector/AutomationInspectorWidget.hpp>

namespace Automation
{
class InspectorFactory final
    : public Process::
          InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  ISCORE_CONCRETE("2c5410ba-d3df-45b8-8444-6d8578b4e1a8")
};
}
