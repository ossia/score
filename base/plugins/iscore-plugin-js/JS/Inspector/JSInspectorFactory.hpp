#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <JS/JSProcessModel.hpp>
#include <JS/JSStateProcess.hpp>
#include <JS/Inspector/JSInspectorWidget.hpp>

namespace JS
{
class InspectorFactory final :
        public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
        ISCORE_CONCRETE_FACTORY("035923ae-1cbf-4ca8-97bd-cf6205ca396e")
};

class StateInspectorFactory final :
        public Process::StateProcessInspectorWidgetDelegateFactory_T<StateProcess, StateInspectorWidget>
{
        ISCORE_CONCRETE_FACTORY("5f31a70f-94f1-489d-ac96-55d36d7d81e8")
};
}
