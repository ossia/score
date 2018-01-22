// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventClassicFactory.hpp>
#include <Scenario/Process/Algorithms/GoodOldDisplacementPolicy.hpp>
#include <Scenario/Process/Algorithms/ConstrainedDisplacementPolicy.hpp>

#include <QString>
#include <algorithm>
#include <score/tools/std/Optional.hpp>

#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/path/Path.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/Identifier.hpp>

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
    ExpandMode mode,
    LockMode lck)
{
  if(lck == LockMode::Free)
  {
    return std::make_unique<MoveEvent<GoodOldDisplacementPolicy>>(
                                                                   std::move(scenarioPath),
                                                                   std::move(eventId),
                                                                   std::move(newDate),
                                                                   mode,
                                                                   lck);
  }
  else
  {
    return std::make_unique<MoveEvent<ConstrainedDisplacementPolicy>>(
                                                                   std::move(scenarioPath),
                                                                   std::move(eventId),
                                                                   std::move(newDate),
                                                                   mode,
                                                                   lck);
  }
}

std::unique_ptr<SerializableMoveEvent> MoveEventClassicFactory::make(LockMode lck)
{
  if(lck == LockMode::Free)
  {
    return std::make_unique<MoveEvent<GoodOldDisplacementPolicy>>();
  }
  else
  {
    return std::make_unique<MoveEvent<ConstrainedDisplacementPolicy>>();
  }
}
}
}
