#pragma once

#include <iscore/command/AggregateCommand.hpp>
#include <Commands/State/UpdateState.hpp>
#include "Commands/Scenario/Creations/CreateState.hpp"
#include "Document/Event/EventModel.hpp"

namespace Scenario
{
    namespace Command
    {
        class AddStateWithData: public iscore::AggregateCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), "AddStateWithData", "AddStateWithData")
            public:
                AddStateWithData():
                      AggregateCommand{factoryName(),
                                       commandName(),
                                       description()} { }

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
                    addCommand(new UpdateState{
                                   std::move(Path),
                                   stateData});
                }
        };
    }
}
