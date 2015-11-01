#include <MoveEventCSPFactory.hpp>
#include <CSPDisplacementPolicy.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>

SerializableMoveEvent* MoveEventCSPFactory::make(
        Path<ScenarioModel> &&scenarioPath,
        const Id<EventModel> &eventId,
        const TimeValue &newDate,
        ExpandMode mode)
{
    return new MoveEvent<CSPDisplacementPolicy>(std::move(scenarioPath), eventId, newDate, mode);
}

SerializableMoveEvent* MoveEventCSPFactory::make()
{
    return new MoveEvent<CSPDisplacementPolicy>();
}
