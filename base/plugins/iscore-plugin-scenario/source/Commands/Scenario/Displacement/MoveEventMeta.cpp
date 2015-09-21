#include "MoveEventMeta.hpp"
#include <Commands/Scenario/Displacement/MoveEventList.hpp>

MoveEventMeta::MoveEventMeta(
        Path<ScenarioModel>&& scenarioPath,
        const Id<EventModel>& eventId,
        const TimeValue& newDate,
        ExpandMode mode)
    :iscore::SerializableCommand{factoryName(), commandName(), description()}
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
