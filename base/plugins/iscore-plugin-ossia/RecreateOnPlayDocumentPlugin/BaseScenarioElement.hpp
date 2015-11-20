#pragma once

#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <memory>

#include "ConstraintElement.hpp"
#include "TimeNodeElement.hpp"
#include "EventElement.hpp"
#include "StateElement.hpp"

class EventModel;
class ConstraintModel;
class TimeNodeModel;
class BaseScenario;
namespace OSSIA
{
    class Scenario;
    class TimeProcess;
}


namespace RecreateOnPlay
{

class BaseScenarioElement final : public QObject
{
    public:
        BaseScenarioElement(
                const BaseScenario& element,
                QObject* parent);

        ConstraintElement* baseConstraint() const;

        TimeNodeElement* startTimeNode() const;
        TimeNodeElement* endTimeNode() const;

        EventElement* startEvent() const;
        EventElement* endEvent() const;

        StateElement* startState() const;
        StateElement* endState() const;

    private:
        ConstraintElement* m_ossia_constraint{};

        TimeNodeElement* m_ossia_startTimeNode{};
        TimeNodeElement* m_ossia_endTimeNode{};

        EventElement* m_ossia_startEvent{};
        EventElement* m_ossia_endEvent{};

        StateElement* m_ossia_startState{};
        StateElement* m_ossia_endState{};

        const DeviceList& m_deviceList;
};
}
