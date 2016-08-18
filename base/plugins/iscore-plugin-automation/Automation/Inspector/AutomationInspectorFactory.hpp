#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <Automation/Inspector/AutomationInspectorWidget.hpp>
#include <Automation/AutomationModel.hpp>

namespace Automation
{
class InspectorFactory final :
        public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
        ISCORE_CONCRETE_FACTORY("2c5410ba-d3df-45b8-8444-6d8578b4e1a8")
};
}
