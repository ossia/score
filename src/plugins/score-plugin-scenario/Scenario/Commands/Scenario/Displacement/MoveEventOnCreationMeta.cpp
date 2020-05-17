// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MoveEventOnCreationMeta.hpp"

#include "MoveEventFactoryInterface.hpp"

#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>
#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <QByteArray>

namespace Scenario
{
class EventModel;
class ProcessModel;
namespace Command
{

MoveEventOnCreationMeta::MoveEventOnCreationMeta(
    const Scenario::ProcessModel& scenarioPath,
    Id<EventModel> eventId,
    TimeVal newDate,
    ExpandMode mode)
    : SerializableMoveEvent{}
    , m_moveEventImplementation(
          score::AppContext()
              .interfaces<MoveEventList>()
              .get(score::AppContext(), MoveEventFactoryInterface::Strategy::CREATION)
              .make(scenarioPath, std::move(eventId), std::move(newDate), mode, LockMode::Free))
{
}

MoveEventOnCreationMeta::~MoveEventOnCreationMeta() { }

void MoveEventOnCreationMeta::undo(const score::DocumentContext& ctx) const
{
  m_moveEventImplementation->undo(ctx);
}

void MoveEventOnCreationMeta::redo(const score::DocumentContext& ctx) const
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
      = score::AppContext()
            .interfaces<MoveEventList>()
            .get(score::AppContext(), MoveEventFactoryInterface::Strategy::CREATION)
            .make(LockMode::Free);

  m_moveEventImplementation->deserialize(cmdData);
}

void MoveEventOnCreationMeta::update(
    Scenario::ProcessModel& scenario,
    const Id<EventModel>& eventId,
    const TimeVal& newDate,
    double y,
    ExpandMode mode,
    LockMode lm)
{
  m_moveEventImplementation->update(scenario, eventId, newDate, y, mode, LockMode::Free);
}
}
}
