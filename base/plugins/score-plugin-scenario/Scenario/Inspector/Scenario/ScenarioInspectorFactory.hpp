#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <Scenario/Inspector/Scenario/ScenarioInspectorWidget.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

namespace Scenario
{
class ScenarioInspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<
          ProcessModel, ScenarioInspectorWidget>
{
  SCORE_CONCRETE("2d6e103e-6136-43cc-9948-57de2cdf8f31")
};
}
