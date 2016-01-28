#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

namespace Scenario
{
namespace Command
{

class ScenarioPasteContent final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), ScenarioPasteContent, "Paste content in scenario")
};

}
}
