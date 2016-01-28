#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventClassicFactory.hpp>
#include <Scenario/Process/Algorithms/GoodOldDisplacementPolicy.hpp>

#include <boost/optional/optional.hpp>
#include <QString>
#include <algorithm>

#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

namespace Scenario
{
class ScenarioModel;
class EventModel;
namespace Command
{
class SerializableMoveEvent;

SerializableMoveEvent* MoveEventClassicFactory::make(
        Path<Scenario::ScenarioModel>&& scenarioPath,
        const Id<EventModel>& eventId,
        const TimeValue& newDate,
        ExpandMode mode)
{
    return new MoveEvent<GoodOldDisplacementPolicy>(std::move(scenarioPath), eventId, newDate, mode);
}

SerializableMoveEvent* MoveEventClassicFactory::make()
{
    return new MoveEvent<GoodOldDisplacementPolicy>();
}

const MoveEventFactoryKey& MoveEventClassicFactory::concreteFactoryKey() const
{
    static const MoveEventFactoryKey str{"644a6f8d-de63-4951-b28b-33b5e2c71ac8"};
    return str;
}
}
}
