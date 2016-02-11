#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>

#include <QByteArray>
#include <algorithm>

#include "MoveEventMeta.hpp"
#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>

#include <iscore/tools/SettableIdentifier.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>

namespace Scenario
{
class EventModel;
namespace Command
{
MoveEventMeta::MoveEventMeta(Path<Scenario::ScenarioModel>&& scenarioPath,
        const Id<EventModel>& eventId,
        const TimeValue& newDate, double y,
        ExpandMode mode)
    :SerializableMoveEvent{},
     m_strategy{MoveEventFactoryInterface::Strategy::MOVING_CLASSIC},
     m_scenario{scenarioPath},
     m_eventId{eventId},
     m_oldY{y},
     m_moveEventImplementation(
         context.components.factory<MoveEventList>()
         .get(m_strategy)
         .make(std::move(scenarioPath), eventId, newDate, mode))
{
    auto& scenar = m_scenario.find();
    auto& ev = scenar.event(m_eventId);
    auto states = ev.states();
    if(states.size() == 1)
    {
        auto& st = scenar.states.at(states.front());
        m_oldY = st.heightPercentage();
    }
}

void MoveEventMeta::undo() const
{
    m_moveEventImplementation->undo();
    auto& scenar = m_scenario.find();
    auto& ev = scenar.event(m_eventId);
    auto states = ev.states();
    if(states.size() == 1)
    {
        auto& st = scenar.states.at(states.front());
        if(st.previousConstraint())
            updateConstraintVerticalPos(m_oldY, st.previousConstraint(), scenar);
        if(st.nextConstraint())
            updateConstraintVerticalPos(m_oldY, st.nextConstraint(), scenar);
        if(!st.previousConstraint() && !st.nextConstraint())
            st.setHeightPercentage(m_oldY);
    }
}

void MoveEventMeta::redo() const
{
    m_moveEventImplementation->redo();
    if(!m_scenario.valid())
        return;
    auto& scenar = m_scenario.find();
    auto& ev = scenar.event(m_eventId);
    auto states = ev.states();
    if(states.size() == 1)
    {
        auto& st = scenar.states.at(states.front());
        if(st.previousConstraint())
            updateConstraintVerticalPos(m_newY, st.previousConstraint(), scenar);
        if(st.nextConstraint())
            updateConstraintVerticalPos(m_newY, st.nextConstraint(), scenar);
        if(!st.previousConstraint() && !st.nextConstraint())
            st.setHeightPercentage(m_newY);
    }
}

const Path<Scenario::ScenarioModel>&MoveEventMeta::path() const
{
    return m_moveEventImplementation->path();
}

void MoveEventMeta::serializeImpl(DataStreamInput& qDataStream) const
{
    qDataStream << m_moveEventImplementation->serialize();
}

void MoveEventMeta::deserializeImpl(DataStreamOutput& qDataStream)
{
    QByteArray cmdData;

    qDataStream >> cmdData;

    m_moveEventImplementation =
            context.components.factory<MoveEventList>()
            .get(m_strategy).make();

    m_moveEventImplementation->deserialize(cmdData);
}

void MoveEventMeta::update(const Path<Scenario::ScenarioModel>& scenarioPath,
                           const Id<EventModel>& eventId,
                           const TimeValue& newDate, double y,
                           ExpandMode mode)
{
    m_moveEventImplementation->update(scenarioPath, eventId, newDate, y, mode);
    m_newY = y;
}
}
}
