#include <Scenario/Commands/Scenario/Displacement/MoveEventClassicFactory.hpp>
#include <Scenario/Process/Algorithms/GoodOldDisplacementPolicy.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>

SerializableMoveEvent* MoveEventClassicFactory::make(
        Path<ScenarioModel>&& scenarioPath,
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
