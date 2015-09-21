#pragma once

#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <ProcessInterface/ExpandMode.hpp>

class ScenarioModel;
class EventModel;

class MoveEventFactoryInterface : public iscore::FactoryInterface
{
public:
    virtual iscore::SerializableCommand* make(
            Path<ScenarioModel>&& scenarioPath,
            const Id<EventModel>& eventId,
            const TimeValue& newDate,
            ExpandMode mode) = 0;

    virtual iscore::SerializableCommand* make() = 0;

    virtual ~MoveEventFactoryInterface() = default;

    /**
     * @brief priority
     * the highest priority will be default move behavior
     * @return
     * the priority
     */
    virtual int priority() =0;

    static QString factoryName()
    {
        return "MoveEvent";
    }

    virtual QString name() const = 0;
};
