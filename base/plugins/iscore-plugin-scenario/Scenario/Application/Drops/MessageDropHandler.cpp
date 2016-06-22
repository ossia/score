#include "MessageDropHandler.hpp"

#include <Scenario/Commands/Scenario/Creations/CreateStateMacro.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeNode_Event_State.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>

#include <State/MessageListSerialization.hpp>

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>


#include <QMimeData>

namespace Scenario
{
bool MessageDropHandler::handle(
        const Scenario::TemporalScenarioPresenter & pres,
        QPointF pos,
        const QMimeData *mime)
{
    using namespace Scenario::Command;
    // If the mime data has states in it we can handle it.
    if(!mime->formats().contains(iscore::mime::messagelist()))
        return false;

    Mime<State::MessageList>::Deserializer des{*mime};
    State::MessageList ml = des.deserialize();

    MacroCommandDispatcher m(
                new Scenario::Command::CreateStateMacro,
                pres.context().context.commandStack);


    const Scenario::ScenarioModel& scenar = pres.processModel();
    Id<StateModel> createdState;

    Scenario::Point pt = pres.toScenarioPoint(pos);

    auto state = furthestSelectedState(scenar);
    if(state && (scenar.events.at(state->eventId()).date() < pt.date))
    {
        if(state->nextConstraint())
        {
            // We create from the event instead
            auto cmd1 = new Scenario::Command::CreateState{scenar, state->eventId(), pt.y};
            m.submitCommand(cmd1);

            auto cmd2 = new Scenario::Command::CreateConstraint_State_Event_TimeNode{
                    scenar, cmd1->createdState(), pt.date, pt.y};
            m.submitCommand(cmd2);
            createdState = cmd2->createdState();
        }
        else
        {
            auto cmd = new Scenario::Command::CreateConstraint_State_Event_TimeNode{
                    scenar, state->id(), pt.date, state->heightPercentage()};
            m.submitCommand(cmd);
            createdState = cmd->createdState();
        }
    }
    else
    {
        // We create in the emptiness
        auto cmd = new Scenario::Command::CreateTimeNode_Event_State(
                    scenar, pt.date, pt.y);
        m.submitCommand(cmd);
        createdState = cmd->createdState();
    }

    auto state_path = make_path(scenar)
            .extend(createdState)
            .extend(Id<MessageItemModel>{});

    auto cmd2 = new AddMessagesToState{
            std::move(state_path),
            ml};

    m.submitCommand(cmd2);

    m.commit();

    return true;
}
}
