// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventClassicFactory.hpp>
#include <Scenario/Process/Algorithms/GoodOldDisplacementPolicy.hpp>
#include <Scenario/Process/Algorithms/ConstrainedDisplacementPolicy.hpp>

#include <QString>
#include <algorithm>
#include <iscore/tools/std/Optional.hpp>

#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
class ProcessModel;
class EventModel;
namespace Command
{
class SerializableMoveEvent;


std::unique_ptr<SerializableMoveEvent> MoveEventClassicFactory::make(
    const Scenario::ProcessModel& scenarioPath,
    Id<EventModel>
        eventId,
    TimeVal newDate,
    ExpandMode mode)
{
  return std::make_unique<MoveEvent<ConstrainedDisplacementPolicy>>(
      std::move(scenarioPath), std::move(eventId), std::move(newDate), mode);
}

std::unique_ptr<SerializableMoveEvent> MoveEventClassicFactory::make()
{
  return std::make_unique<MoveEvent<ConstrainedDisplacementPolicy>>();
}
}
}
