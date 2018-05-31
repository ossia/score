#pragma once
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>

namespace Scenario
{
class IntervalModel;
namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT Encapsulate final
    : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(ScenarioCommandFactoryName(), Encapsulate, "Encapsulate")
};
}

struct EncapsData
{
  double topY{}, bottomY{};
  IntervalModel* interval{};
};

EncapsData EncapsulateElements(
    Scenario::Command::Macro& disp,
    CategorisedScenario& cat,
    const ProcessModel& scenar);

void EncapsulateInScenario(
    const ProcessModel& scenar, const score::CommandStackFacade& stack);
void Duplicate(
    const ProcessModel& scenar, const score::CommandStackFacade& stack);
}
