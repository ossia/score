#pragma once
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>

#include <QObject>
namespace score
{
class CommandStackFacade;
}
namespace Scenario
{
namespace Command
{
class Macro;
}
class IntervalModel;
namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT Encapsulate final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), Encapsulate, "Encapsulate")
};
class SCORE_PLUGIN_SCENARIO_EXPORT Decapsulate final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), Decapsulate, "Decapsulate")
};
}

struct EncapsData
{
  double topY{}, bottomY{};
  IntervalModel* interval{};
};

SCORE_PLUGIN_SCENARIO_EXPORT
EncapsData EncapsulateElements(
    Scenario::Command::Macro& disp,
    CategorisedScenario& cat,
    const ProcessModel& scenar);

// Encapsulate the selected elements of scenar in a new sub-scenario
void EncapsulateInScenario(const ProcessModel& scenar, const score::CommandStackFacade& stack);

// Decpasulate a sub-scenar passed in argument in its parent scenario if
// possible
void DecapsulateScenario(const ProcessModel& scenar, const score::CommandStackFacade& stack);

void Duplicate(const ProcessModel& scenar, const score::CommandStackFacade& stack);
}
