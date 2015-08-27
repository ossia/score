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
#include <QPointer>
class EventModel;
class ConstraintModel;
class TimeNodeModel;
class ScenarioModel;
class DeviceList;
class OSSIAConstraintElement;

namespace OSSIA
{
    class Scenario;
}

class OSSIAScenarioElement : public OSSIAProcessElement
{
    public:
        OSSIAScenarioElement(
                OSSIAConstraintElement* parentConstraint,
                const ScenarioModel* element,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeProcess> process() const override;
        std::shared_ptr<OSSIA::Scenario> scenario() const;

        const auto& states() const
        { return m_ossia_states; }

        const Process* iscoreProcess() const override;

        void stop() override;

    private:
        void on_constraintCreated(const ConstraintModel& id);
        void on_stateCreated(const StateModel& id);
        void on_eventCreated(const EventModel& id);
        void on_timeNodeCreated(const TimeNodeModel& id);

        void on_constraintRemoved(const Id<ConstraintModel>& id);
        void on_stateRemoved(const Id<StateModel>& id);
        void on_eventRemoved(const Id<EventModel>& id);
        void on_timeNodeRemoved(const Id<TimeNodeModel>& id);


        QPointer<OSSIAConstraintElement> m_parent_constraint;

        std::map<Id<ConstraintModel>, OSSIAConstraintElement*> m_ossia_constraints;
        std::map<Id<StateModel>, OSSIAStateElement*> m_ossia_states;
        std::map<Id<TimeNodeModel>, OSSIATimeNodeElement*> m_ossia_timenodes;
        std::map<Id<EventModel>, OSSIAEventElement*> m_ossia_timeevents;
        std::shared_ptr<OSSIA::Scenario> m_ossia_scenario;
        const ScenarioModel* m_iscore_scenario{};

        IdContainer<ConstraintModel> m_executingConstraints;

        const DeviceList& m_deviceList;
};
