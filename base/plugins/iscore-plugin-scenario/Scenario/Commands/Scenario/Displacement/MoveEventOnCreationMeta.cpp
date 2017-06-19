#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>

#include <QByteArray>
#include <algorithm>

#include "MoveEventFactoryInterface.hpp"
#include "MoveEventOnCreationMeta.hpp"
#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
class EventModel;
class ProcessModel;
namespace Command
{

MoveEventOnCreationMeta::MoveEventOnCreationMeta(
    const Scenario::ProcessModel& scenarioPath,
    Id<EventModel>
        eventId,
    TimeVal newDate,
    ExpandMode mode)
    : SerializableMoveEvent{}
    , m_moveEventImplementation(
          context.interfaces<MoveEventList>()
              .get(context, MoveEventFactoryInterface::Strategy::CREATION)
              .make(
                  scenarioPath, std::move(eventId),
                  std::move(newDate), mode))
{
}

MoveEventOnCreationMeta::~MoveEventOnCreationMeta()
{
}

void MoveEventOnCreationMeta::undo(const iscore::DocumentContext& ctx) const
{
  m_moveEventImplementation->undo(ctx);
}

void MoveEventOnCreationMeta::redo(const iscore::DocumentContext& ctx) const
{
  m_moveEventImplementation->redo(ctx);
}

const Path<Scenario::ProcessModel>& MoveEventOnCreationMeta::path() const
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

  m_moveEventImplementation
      = context.interfaces<MoveEventList>()
            .get(context, MoveEventFactoryInterface::Strategy::CREATION)
            .make();

  m_moveEventImplementation->deserialize(cmdData);
}

void MoveEventOnCreationMeta::update(
    Scenario::ProcessModel& scenario,
    const Id<EventModel>& eventId, const TimeVal& newDate, double y,
    ExpandMode mode)
{
  m_moveEventImplementation->update(scenario, eventId, newDate, y, mode);
}
}
}
