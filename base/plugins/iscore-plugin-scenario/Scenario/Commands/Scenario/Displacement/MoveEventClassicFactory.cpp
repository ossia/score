#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventClassicFactory.hpp>
#include <Scenario/Process/Algorithms/GoodOldDisplacementPolicy.hpp>

#include <iscore/tools/std/Optional.hpp>
#include <QString>
#include <algorithm>

#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

namespace Scenario
{
class ProcessModel;
class EventModel;
namespace Command
{
class SerializableMoveEvent;

std::unique_ptr<SerializableMoveEvent> MoveEventClassicFactory::make(
        Path<Scenario::ProcessModel>&& scenarioPath,
        Id<EventModel> eventId,
        TimeValue newDate,
        ExpandMode mode)
{
    return std::make_unique<MoveEvent<GoodOldDisplacementPolicy>>(
                std::move(scenarioPath),
                std::move(eventId),
                std::move(newDate),
                mode);
}

std::unique_ptr<SerializableMoveEvent> MoveEventClassicFactory::make()
{
    return std::make_unique<MoveEvent<GoodOldDisplacementPolicy>>();
}
}
}
