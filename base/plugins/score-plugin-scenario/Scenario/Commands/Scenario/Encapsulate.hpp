#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/model/Identifier.hpp>

#include <Scenario/Commands/Scenario/Deletions/RemoveSelection.hpp>
#include <Scenario/Commands/Scenario/ScenarioPasteElements.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
namespace Scenario
{
class IntervalModel;
namespace Command
{

class SCORE_PLUGIN_SCENARIO_EXPORT Encapsulate final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), Encapsulate, "Encapsulate")
};

}
void EncapsulateElements(
    const Scenario::ProcessModel& scenar,
    const score::CommandStackFacade& stack);
}
