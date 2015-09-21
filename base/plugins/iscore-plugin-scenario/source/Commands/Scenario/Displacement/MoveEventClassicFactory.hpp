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

    virtual int priority()
    {
        // we set a low priority here to let plugins easily take over
        return 0;
    }

    QString name() const override
    { return "Classic"; }
};
