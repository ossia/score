#pragma once

#include <iscore/command/AggregateCommand.hpp>
#include <Scenario/Commands/State/UpdateState.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateState.hpp>
#include <Scenario/Document/Event/EventModel.hpp>

namespace Scenario
{
    namespace Command
    {
        class AddStateWithData final : public iscore::AggregateCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddStateWithData, "AddStateWithData")
            public:
                AddStateWithData(
                         const ScenarioModel& scenario,
                         const Id<EventModel>& ev,
                         double ypos,
                         iscore::MessageList&& stateData) :
                    AggregateCommand {factoryName(),
                                      commandName(),
                                      description()}
                {
                    auto createStateCmd =  new CreateState{scenario, ev, ypos};

                    // We create the path of the to-be state
                    auto vecpath = createStateCmd->scenarioPath().unsafePath().vec();
                    vecpath.append({"StateModel", createStateCmd->createdState()});
                    vecpath.append({"MessageItemModel", {}});
                    Path<MessageItemModel> Path{ObjectPath(std::move(vecpath)), {}};

                    addCommand(createStateCmd);
                    addCommand(new AddMessagesToState{
                                   std::move(Path),
                                   stateData});
                }
        };
    }
}
