#include <MoveEventCSPFlexFactory.hpp>
#include <CSPFlexDisplacementPolicy.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>

SerializableMoveEvent* MoveEventCSPFlexFactory::make(
        Path<Scenario::ScenarioModel> &&scenarioPath,
        const Id<EventModel> &eventId,
        const TimeValue &newDate,
        ExpandMode mode)
{
    return new MoveEvent<CSPFlexDisplacementPolicy>(std::move(scenarioPath), eventId, newDate, mode);
}

SerializableMoveEvent* MoveEventCSPFlexFactory::make()
{
    return new MoveEvent<CSPFlexDisplacementPolicy>();
}

const MoveEventFactoryKey& MoveEventCSPFlexFactory::key_impl() const
{
    // TODO why is CSP CSPFlex and CSPFlex CSP
    static const MoveEventFactoryKey str{"CSP"};
    return str;
}
