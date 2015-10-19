#pragma once

#include <Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>

class MoveEventClassicFactory : public MoveEventFactoryInterface
{
    // MoveEventFactory interface
public:
    SerializableMoveEvent* make(
            Path<ScenarioModel>&& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            ExpandMode mode) override;

    SerializableMoveEvent* make() override;

    virtual int priority(MoveEventList::Strategy strategy) override
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

    QString name() const override
    { return "Classic"; }
};
