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
            ExpandMode mode);

    SerializableMoveEvent* make();

    virtual int priority(MoveEventList::Strategy strategy)
    {
        switch(strategy)
        {
            default:
                return 0;// not suited for other strategies
                break;
        }
    }

    QString name() const override
    { return "Classic"; }
};
