#pragma once

#include <iscore/command/AggregateCommand.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateState.hpp>
#include <Scenario/Document/Event/EventModel.hpp>

namespace Scenario
{
    namespace Command
    {
        class AddStateWithData final : public iscore::AggregateCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddStateWithData, "Drop a new state in an event")
            public:
                AddStateWithData(
                         const Scenario::ScenarioModel& scenario,
                         const Id<EventModel>& ev,
                         double ypos,
                         iscore::MessageList&& stateData)
                {
                    auto createStateCmd =  new CreateState{scenario, ev, ypos};

                    // We create the path of the to-be state
                    auto path = createStateCmd->scenarioPath()
                            .extend(StateModel::className, createStateCmd->createdState())
                            .extend("MessageItemModel", Id<MessageItemModel>{});

                    addCommand(createStateCmd);
                    addCommand(new AddMessagesToState{
                                   std::move(path),
                                   stateData});
                }
        };
    }
}
