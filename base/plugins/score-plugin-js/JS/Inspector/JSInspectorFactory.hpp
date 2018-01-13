#pragma once
#include <JS/Inspector/JSInspectorWidget.hpp>
#include <JS/JSProcessModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

namespace JS
{
class InspectorFactory final
    : public Process::
          InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("035923ae-1cbf-4ca8-97bd-cf6205ca396e")
};

}
