#pragma once

#include <iscore/plugins/customfactory/FactoryInterface.hpp>
//#include <iscore/command/SerializableCommand.hpp>
#include <Commands/Scenario/Displacement/SerializableMoveEvent.hpp>
#include <Commands/Scenario/Displacement/MoveEventList.hpp>

#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <ProcessInterface/ExpandMode.hpp>

class ScenarioModel;
class EventModel;


class MoveEventFactoryInterface : public iscore::FactoryInterface
{
public:
    virtual SerializableMoveEvent* make(
            Path<ScenarioModel>&& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            ExpandMode mode) = 0;

    virtual SerializableMoveEvent* make() = 0;

    virtual ~MoveEventFactoryInterface() = default;

     /**
     * @brief priority
     * the highest priority will be default move behavior for the indicated strategy
     * Basically, we want to know how well the policy is suited for the desired strategy.
     * @param strategy
     * the strategy for which we need a displacement policy;
     * @return
     */
    virtual int priority(MoveEventList::Strategy strategy) = 0;

    static QString factoryName()
    {
        return "MoveEvent";
    }

    virtual QString name() const = 0;
};
