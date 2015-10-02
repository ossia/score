#include "MoveEventOnCreationMeta.hpp"
#include <Commands/Scenario/Displacement/MoveEventList.hpp>
#include "MoveEventFactoryInterface.hpp"

MoveEventOnCreationMeta::MoveEventOnCreationMeta(
        Path<ScenarioModel>&& scenarioPath,
        const Id<EventModel>& eventId,
        const TimeValue& newDate,
        ExpandMode mode)
    :SerializableMoveEvent{},
     m_moveEventImplementation(MoveEventList::getFactory(MoveEventList::Strategy::MOVING)->make(std::move(scenarioPath), eventId, newDate, mode))
{
}

void MoveEventOnCreationMeta::undo()
{
    m_moveEventImplementation->undo();
}

void MoveEventOnCreationMeta::redo()
{

    m_moveEventImplementation->redo();
}

const Path<ScenarioModel>&MoveEventOnCreationMeta::path() const
{
    return m_moveEventImplementation->path();
}

void MoveEventOnCreationMeta::serializeImpl(QDataStream& qDataStream) const
{
    qDataStream << m_moveEventImplementation->serialize();
}

void MoveEventOnCreationMeta::deserializeImpl(QDataStream& qDataStream)
{
    QByteArray cmdData;

    qDataStream >> cmdData;

    m_moveEventImplementation = MoveEventList::getFactory(MoveEventList::Strategy::MOVING)->make();

    m_moveEventImplementation->deserialize(cmdData);
}

void MoveEventOnCreationMeta::update(const Path<ScenarioModel>& scenarioPath,
                           const Id<EventModel>& eventId,
                           const TimeValue& newDate,
                           ExpandMode mode)
{
    m_moveEventImplementation->update(scenarioPath, eventId, newDate, mode);
}
