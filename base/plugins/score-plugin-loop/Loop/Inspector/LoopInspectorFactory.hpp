#pragma once
#include <Loop/Inspector/LoopInspectorWidget.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

namespace Loop
{
class InspectorFactory final
    : public Process::
          InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("f45f98f2-f721-4ffa-9219-114832fe06bd")
};
}
