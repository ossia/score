#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <memory>

#include <Process/ScenarioModel.hpp>

#include "OSSIAProcessElement.hpp"
#include "OSSIAConstraintElement.hpp"
#include "OSSIATimeNodeElement.hpp"
#include "OSSIAStateElement.hpp"
#include "OSSIAEventElement.hpp"
#include <Editor/TimeEvent.h>
#include <QPointer>
class EventModel;
class ConstraintModel;
class TimeNodeModel;
class ScenarioModel;
class DeviceList;
class OSSIAConstraintElement;

namespace OSSIA
{
    class StateElement;
    class Scenario;
    class TimeValue;
}

// TODO see if this can be used for the base scenario model too.
class OSSIAScenarioElement : public OSSIAProcessElement
{
    public:
        OSSIAScenarioElement(
                OSSIAConstraintElement& parentConstraint,
                ScenarioModel& element,
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

        void on_constraintRemoved(const ConstraintModel&);
        void on_stateRemoved(const StateModel&);
        void on_eventRemoved(const EventModel&);
        void on_timeNodeRemoved(const TimeNodeModel&);

        void startConstraintExecution(const Id<ConstraintModel>&);
        void stopConstraintExecution(const Id<ConstraintModel>&);

        void eventCallback(
                OSSIAEventElement& ev,
                OSSIA::TimeEvent::Status newStatus);

    private:
        // TODO use IdContainer
        std::map<Id<ConstraintModel>, OSSIAConstraintElement*> m_ossia_constraints;
        std::map<Id<StateModel>, OSSIAStateElement*> m_ossia_states;
        std::map<Id<TimeNodeModel>, OSSIATimeNodeElement*> m_ossia_timenodes;
        std::map<Id<EventModel>, OSSIAEventElement*> m_ossia_timeevents;
        std::shared_ptr<OSSIA::Scenario> m_ossia_scenario;
        ScenarioModel& m_iscore_scenario;

        IdContainer<ConstraintModel> m_executingConstraints;

        const DeviceList& m_deviceList;
};
