#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>

#include <QByteArray>
#include <algorithm>

#include "MoveEventFactoryInterface.hpp"
#include "MoveEventOnCreationMeta.hpp"
#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

namespace Scenario
{
class EventModel;
class ScenarioModel;
namespace Command
{

MoveEventOnCreationMeta::MoveEventOnCreationMeta(
        Path<Scenario::ScenarioModel>&& scenarioPath,
        const Id<EventModel>& eventId,
        const TimeValue& newDate,
        ExpandMode mode)
    :SerializableMoveEvent{},
     m_moveEventImplementation(
         context.components.factory<MoveEventList>()
         .get(context, MoveEventFactoryInterface::Strategy::CREATION)
         .make(std::move(scenarioPath), eventId, newDate, mode))
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
            .get(context, MoveEventFactoryInterface::Strategy::CREATION).make();

    m_moveEventImplementation->deserialize(cmdData);
}

void MoveEventOnCreationMeta::update(const Path<Scenario::ScenarioModel>& scenarioPath,
                           const Id<EventModel>& eventId,
                           const TimeValue& newDate, double y,
                           ExpandMode mode)
{
    m_moveEventImplementation->update(scenarioPath, eventId, newDate, y, mode);
}
}
}
