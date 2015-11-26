#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <memory>

#include <Scenario/Process/ScenarioModel.hpp>

#include "ProcessElement.hpp"
#include "ConstraintElement.hpp"
#include "TimeNodeElement.hpp"
#include "StateElement.hpp"
#include "EventElement.hpp"
#include <Editor/TimeEvent.h>
#include <QPointer>
class EventModel;
class ConstraintModel;
class TimeNodeModel;
namespace Scenario { class ScenarioModel; }
class DeviceList;

namespace OSSIA
{
    class StateElement;
    class Scenario;
    class TimeValue;
}

namespace RecreateOnPlay
{
class ConstraintElement;
// TODO see if this can be used for the base scenario model too.
class ScenarioElement final : public ProcessElement
{
    public:
        ScenarioElement(
                ConstraintElement& parentConstraint,
                Scenario::ScenarioModel& element,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeProcess> OSSIAProcess() const override;
        std::shared_ptr<OSSIA::Scenario> scenario() const;

        const auto& states() const
        { return m_ossia_states; }
        const auto& constraints() const
        { return m_ossia_constraints; }
        const auto& events() const
        { return m_ossia_timeevents; }
        const auto& timeNodes() const
        { return m_ossia_timenodes; }

        Process& iscoreProcess() const override;

        void stop() override;

    private:
        void on_constraintCreated(const ConstraintModel&);
        void on_stateCreated(const StateModel&);
        void on_eventCreated(const EventModel&);
        void on_timeNodeCreated(const TimeNodeModel&);

        void startConstraintExecution(const Id<ConstraintModel>&);
        void stopConstraintExecution(const Id<ConstraintModel>&);

        void eventCallback(
                EventElement& ev,
                OSSIA::TimeEvent::Status newStatus);

    private:
        // TODO use IdContainer
        std::map<Id<ConstraintModel>, ConstraintElement*> m_ossia_constraints;
        std::map<Id<StateModel>, StateElement*> m_ossia_states;
        std::map<Id<TimeNodeModel>, TimeNodeElement*> m_ossia_timenodes;
        std::map<Id<EventModel>, EventElement*> m_ossia_timeevents;
        std::shared_ptr<OSSIA::Scenario> m_ossia_scenario;
        Scenario::ScenarioModel& m_iscore_scenario;

        IdContainer<ConstraintModel> m_executingConstraints;

        const DeviceList& m_deviceList;
};
}
