#pragma once

#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>

#include "Process/ExpandMode.hpp"
#include "Process/TimeValue.hpp"

class EventModel;
class SerializableMoveEvent;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario
template <typename Object> class Path;
template <typename tag, typename impl> class id_base_t;

class MoveEventClassicFactory final : public MoveEventFactoryInterface
{
    // MoveEventFactory interface
public:
    SerializableMoveEvent* make(
            Path<Scenario::ScenarioModel>&& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            ExpandMode mode) override;

    SerializableMoveEvent* make() override;

    int priority(MoveEventFactoryInterface::Strategy strategy) override
    {
        switch(strategy)
        {
            case MoveEventFactoryInterface::Strategy::CREATION:
            case MoveEventFactoryInterface::Strategy::MOVING:
            case MoveEventFactoryInterface::Strategy::EXTRA:
            default:
                return 0;// not suited for other strategies
                break;
        }
    }

    const MoveEventFactoryKey& key_impl() const override;
};
