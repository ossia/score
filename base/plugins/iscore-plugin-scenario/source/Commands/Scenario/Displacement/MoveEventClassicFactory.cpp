#include <Commands/Scenario/Displacement/MoveEventClassicFactory.hpp>
#include <Process/Algorithms/StandardDisplacementPolicy.hpp>
#include <Commands/Scenario/Displacement/MoveEvent.hpp>

iscore::SerializableCommand* MoveEventClassicFactory::make(
        Path<ScenarioModel>&& scenarioPath,
        const Id<EventModel>& eventId,
        const TimeValue& newDate,
        ExpandMode mode)
{
    return new MoveEvent<GoodOldDisplacementPolicy>(std::move(scenarioPath), eventId, newDate, mode);
}
