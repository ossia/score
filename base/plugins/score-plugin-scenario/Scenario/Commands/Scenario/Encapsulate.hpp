#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <score/command/AggregateCommand.hpp>
namespace score { class CommandStackFacade; }
namespace Scenario
{
namespace Command { class Macro; }
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
