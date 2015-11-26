#include "MoveEventOnCreationMeta.hpp"
#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>
#include "MoveEventFactoryInterface.hpp"
#include <core/application/ApplicationComponents.hpp>

MoveEventOnCreationMeta::MoveEventOnCreationMeta(
        Path<Scenario::ScenarioModel>&& scenarioPath,
        const Id<EventModel>& eventId,
        const TimeValue& newDate,
        ExpandMode mode)
    :SerializableMoveEvent{},
     m_moveEventImplementation(
         context.components.factory<MoveEventList>()
         .get(MoveEventFactoryInterface::Strategy::MOVING)
         ->make(std::move(scenarioPath), eventId, newDate, mode))
{
}

void MoveEventOnCreationMeta::undo() const
{
    m_moveEventImplementation->undo();
}

void MoveEventOnCreationMeta::redo() const
{

    m_moveEventImplementation->redo();
}

const Path<Scenario::ScenarioModel>&MoveEventOnCreationMeta::path() const
{
    return m_moveEventImplementation->path();
}

void MoveEventOnCreationMeta::serializeImpl(DataStreamInput& qDataStream) const
{
    qDataStream << m_moveEventImplementation->serialize();
}

void MoveEventOnCreationMeta::deserializeImpl(DataStreamOutput& qDataStream)
{
    QByteArray cmdData;

    qDataStream >> cmdData;

    m_moveEventImplementation =
            context.components.factory<MoveEventList>()
            .get(MoveEventFactoryInterface::Strategy::MOVING)->make();

    m_moveEventImplementation->deserialize(cmdData);
}

void MoveEventOnCreationMeta::update(const Path<Scenario::ScenarioModel>& scenarioPath,
                           const Id<EventModel>& eventId,
                           const TimeValue& newDate,
                           ExpandMode mode)
{
    m_moveEventImplementation->update(scenarioPath, eventId, newDate, mode);
}
