#pragma once
#include <iscore/command/AggregateCommand.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

class ScenarioPasteContent final : public iscore::AggregateCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(),
                                      ScenarioPasteContent,
                                      "ScenarioPasteContent")
};
