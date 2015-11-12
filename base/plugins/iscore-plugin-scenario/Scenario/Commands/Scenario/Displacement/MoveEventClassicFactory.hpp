#pragma once

#include <Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>

class MoveEventClassicFactory final : public MoveEventFactoryInterface
{
    // MoveEventFactory interface
public:
    SerializableMoveEvent* make(
            Path<ScenarioModel>&& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            ExpandMode mode) override;

    SerializableMoveEvent* make() override;

    int priority(MoveEventList::Strategy strategy) override
    {
        switch(strategy)
        {
            case MoveEventList::Strategy::CREATION:
            case MoveEventList::Strategy::MOVING:
            case MoveEventList::Strategy::EXTRA:
            default:
                return 0;// not suited for other strategies
                break;
        }
    }

    const std::string& key_impl() const override
    {
        static std::string str{"Classic"};
        return str;
    }
};
