#include <MoveEventCSPFactory.hpp>
#include <CSPDisplacementPolicy.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>

SerializableMoveEvent* MoveEventCSPFactory::make(
        Path<Scenario::ScenarioModel> &&scenarioPath,
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

const MoveEventFactoryKey& MoveEventCSPFactory::key_impl() const
{
    static const MoveEventFactoryKey str{"CSPFlex"};
    return str;
}
