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
                new  Scenario::Command::CreateStateMacro,
                pres.context().context.commandStack);

    const Scenario::ScenarioModel& scenar = ::model(static_cast<const TemporalScenarioLayerModel&>(pres.layerModel()));
    Id<StateModel> createdState;
    auto t = TimeValue::fromMsecs(pos.x() * pres.zoomRatio());
    auto y = pos.y() / (pres.view().boundingRect().size().height() + 150);

    auto state = furthestSelectedState(scenar);
    if(state && (scenar.events.at(state->eventId()).date() < t))
    {
        if(state->nextConstraint())
        {
            // We create from the event instead
            auto cmd1 = new Scenario::Command::CreateState{scenar, state->eventId(), y};
            m.submitCommand(cmd1);

            auto cmd2 = new Scenario::Command::CreateConstraint_State_Event_TimeNode{
                    scenar, cmd1->createdState(), t, y};
            m.submitCommand(cmd2);
            createdState = cmd2->createdState();
        }
        else
        {
            auto cmd = new Scenario::Command::CreateConstraint_State_Event_TimeNode{
                    scenar, state->id(), t, state->heightPercentage()};
            m.submitCommand(cmd);
            createdState = cmd->createdState();
        }
    }
    else
    {
        // We create in the emptiness
        auto cmd = new Scenario::Command::CreateTimeNode_Event_State(
                    scenar, t, y);
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
