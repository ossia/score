#include <MoveEventCSPFactory.hpp>
#include <CSPDisplacementPolicy.hpp>
#include <Commands/Scenario/Displacement/MoveEvent.hpp>

iscore::SerializableCommand* MoveEventCSPFactory::make(Path<ScenarioModel> &&scenarioPath, const Id<EventModel> &eventId, const TimeValue &newDate, ExpandMode mode)
{
    return new MoveEvent<CSPDisplacementPolicy>(std::move(scenarioPath), eventId, newDate, mode);
}

SerializableCommand*MoveEventCSPFactory::make()
{
    return new MoveEvent<CSPDisplacementPolicy>();
}
