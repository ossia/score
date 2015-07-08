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
                ISCORE_COMMAND_DECL("AddStateWithData", "AddStateWithData")
            public:
                AddStateWithData():
                      AggregateCommand{"ScenarioControl",
                                       commandName(),
                                       description()} { }

                AddStateWithData(const ScenarioModel& scenario,
                         const id_type<EventModel>& ev,
                         double ypos,
                         iscore::State&& stateData) :
                    AggregateCommand {"ScenarioControl",
                                      commandName(),
                                      description()}
                {
                    auto createStateCmd =  new CreateState{scenario, ev, ypos};

                    // We create the path of the to-be state
                    auto vecpath = createStateCmd->scenarioPath().vec();
                    vecpath.append({"StateModel", createStateCmd->createdState()});

                    addCommand(createStateCmd);
                    addCommand(new AddStateToStateModel(ObjectPath(std::move(vecpath)), std::move(stateData)));
                }
        };
    }
}
