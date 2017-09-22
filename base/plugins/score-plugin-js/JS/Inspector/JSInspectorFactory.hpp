#pragma once
#include <JS/Inspector/JSInspectorWidget.hpp>
#include <JS/JSProcessModel.hpp>
#include <JS/JSStateProcess.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

namespace JS
{
class InspectorFactory final
    : public Process::
          InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("035923ae-1cbf-4ca8-97bd-cf6205ca396e")
};

class StateInspectorFactory final
    : public Process::
          StateProcessInspectorWidgetDelegateFactory_T<StateProcess, StateInspectorWidget>
{
  SCORE_CONCRETE("5f31a70f-94f1-489d-ac96-55d36d7d81e8")
};
}
