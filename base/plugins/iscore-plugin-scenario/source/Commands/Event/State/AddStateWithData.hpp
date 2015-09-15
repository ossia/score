#pragma once

#include <iscore/command/AggregateCommand.hpp>
#include "Commands/Event/AddStateToEvent.hpp"
#include "Commands/Scenario/Creations/CreateState.hpp"
#include "Document/Event/EventModel.hpp"
namespace Scenario
{
    namespace Command
    {
        class AddStateWithData: public iscore::AggregateCommand
        {
                ISCORE_COMMAND_DECL("ScenarioControl", "AddStateWithData", "AddStateWithData")
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
                    Path<StateModel> Path{ObjectPath(std::move(vecpath)), {}};

                    addCommand(createStateCmd);
                    addCommand(new AddStateToStateModel{
                                   std::move(Path),
                                   iscore::StatePath{},
                                   {iscore::StateData{std::move(stateData), "NewState_other"}, nullptr},
                                   -1});
                }
        };
    }
}
