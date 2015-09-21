#include "MoveEventMeta.hpp"
#include <Commands/Scenario/Displacement/MoveEventList.hpp>

MoveEventMeta::MoveEventMeta(
        Path<ScenarioModel>&& scenarioPath,
        const Id<EventModel>& eventId,
        const TimeValue& newDate,
        ExpandMode mode)
    :SerializableMoveEvent{}
{
}

void MoveEventMeta::undo()
{
    m_moveEventImplementation->undo();
}

void MoveEventMeta::redo()
{
    m_moveEventImplementation->redo();
}

void MoveEventMeta::serializeImpl(QDataStream& qDataStream) const
{
    qDataStream << m_moveEventImplementation->serialize();
}

void MoveEventMeta::deserializeImpl(QDataStream& qDataStream)
{
    QByteArray cmdData;

    qDataStream >> cmdData;

    m_moveEventImplementation = MoveEventList::getFactory()->make();

    m_moveEventImplementation->deserialize(cmdData);
}

void MoveEventMeta::update(const Path<ScenarioModel>& scenarioPath,
                           const Id<EventModel>& eventId,
                           const TimeValue& newDate,
                           ExpandMode mode)
{
    m_moveEventImplementation->update(scenarioPath, eventId, newDate, mode);
}
