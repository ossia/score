#pragma once
#include <Automation/AutomationModel.hpp>
#include <Automation/Inspector/AutomationInspectorWidget.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

namespace Automation
{
class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("2c5410ba-d3df-45b8-8444-6d8578b4e1a8")
};
}

namespace Gradient
{
class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("7d61cbcb-7980-4d50-86c9-3d54a0299fc5")
};
}

namespace Spline
{
class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("94c5afc5-c8c3-44e2-b8b4-6e53d8518677")
};
}

namespace Metronome
{
class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("4c7c9f7d-50ff-4443-ae85-d2c340e17c44")
};
}
