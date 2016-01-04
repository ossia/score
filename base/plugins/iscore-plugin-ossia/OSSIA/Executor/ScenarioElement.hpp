#pragma once
#include <Editor/TimeEvent.h>
#include <boost/optional/optional.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <map>
#include <memory>

#include "ProcessElement.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

class DeviceList;
class EventModel;
namespace Process { class ProcessModel; }
class QObject;
class StateModel;
class TimeNodeModel;
namespace OSSIA {
class TimeProcess;
}  // namespace OSSIA
namespace RecreateOnPlay {
class EventElement;
class StateElement;
class TimeNodeElement;
}  // namespace RecreateOnPlay
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario

namespace OSSIA
{
    class Scenario;
}

namespace RecreateOnPlay
{
class ConstraintElement;

// TODO see if this can be used for the base scenario model too.
class ScenarioElement final : public ProcessComponent
{
    public:
        ScenarioElement(
                ConstraintElement& cst,
                Scenario::ScenarioModel& proc,
                const Context& ctx,
                const Id<iscore::Component>& id,
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

        Process::ProcessModel& iscoreProcess() const override;

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
        const Key &key() const override;
        // TODO use IdContainer
        std::map<Id<ConstraintModel>, ConstraintElement*> m_ossia_constraints;
        std::map<Id<StateModel>, StateElement*> m_ossia_states;
        std::map<Id<TimeNodeModel>, TimeNodeElement*> m_ossia_timenodes;
        std::map<Id<EventModel>, EventElement*> m_ossia_timeevents;
        std::shared_ptr<OSSIA::Scenario> m_ossia_scenario;
        Scenario::ScenarioModel& m_iscore_scenario;

        IdContainer<ConstraintModel> m_executingConstraints;

        const Context& m_ctx;
};


class ScenarioComponentFactory final :
        public ProcessComponentFactory
{
    public:
        virtual ~ScenarioComponentFactory();
        virtual ProcessComponent* make(
                ConstraintElement& cst,
                Process::ProcessModel& proc,
                const Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent) const override;

        const factory_key_type& key_impl() const override;

        bool matches(
                Process::ProcessModel&,
                const DocumentPlugin&,
                const iscore::DocumentContext &) const override;
};

}
