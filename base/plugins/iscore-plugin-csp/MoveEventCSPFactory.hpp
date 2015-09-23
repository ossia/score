#pragma once

#include <Commands/Scenario/Displacement/MoveEventFactoryInterface.hpp>


class MoveEventCSPFactory : public MoveEventFactoryInterface
{
    // MoveEventFactory interface
public:
    SerializableMoveEvent* make(Path<ScenarioModel> &&scenarioPath, const Id<EventModel> &eventId, const TimeValue &newDate, ExpandMode mode);

    SerializableMoveEvent* make();

    virtual int priority()//TODO make higher priority to use it when it will be working
    {
         return 1;
    }

    QString name() const override
    { return "CSP"; }
};
