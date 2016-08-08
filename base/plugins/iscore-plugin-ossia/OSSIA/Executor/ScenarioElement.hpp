#pragma once
#include <ossia/editor/scenario/time_event.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <map>
#include <memory>

#include "ProcessElement.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Tools/dataStructures.hpp>

namespace Device
{
class DeviceList;
}
namespace Process { class ProcessModel; }
class QObject;
namespace ossia {
class time_process;
class time_value;
}  // namespace OSSIA
namespace Engine { namespace Execution {
class EventElement;
class StateElement;
class TimeNodeElement;
} } // namespace RecreateOnPlay
namespace Scenario {
class ProcessModel;
class EventModel;
class StateModel;
class TimeNodeModel;
class CSPCoherencyCheckerInterface;
}  // namespace Scenario

namespace ossia
{
    class scenario;
}

namespace Engine { namespace Execution
{
class ConstraintElement;

// TODO see if this can be used for the base scenario model too.
class ScenarioComponent final :
        public ProcessComponent_T<Scenario::ProcessModel, ossia::scenario>
{
        COMPONENT_METADATA("4e4b1c1a-1a2a-4ae6-a1a1-38d0900e74e8")
    public:
        ScenarioComponent(
                ConstraintElement& cst,
                Scenario::ProcessModel& proc,
                const Context& ctx,
                const Id<iscore::Component>& id,
                QObject* parent);

        const auto& states() const
        { return m_ossia_states; }
        const auto& constraints() const
        { return m_ossia_constraints; }
        const auto& events() const
        { return m_ossia_timeevents; }
        const auto& timeNodes() const
        { return m_ossia_timenodes; }

        void stop() override;

    private:
        void on_constraintCreated(const Scenario::ConstraintModel&);
        void on_stateCreated(const Scenario::StateModel&);
        void on_eventCreated(const Scenario::EventModel&);
        void on_timeNodeCreated(const Scenario::TimeNodeModel&);

        void startConstraintExecution(const Id<Scenario::ConstraintModel>&);
        void stopConstraintExecution(const Id<Scenario::ConstraintModel>&);
        void disableConstraintExecution(const Id<Scenario::ConstraintModel>& id);

        void eventCallback(
                EventElement& ev,
                ossia::time_event::Status newStatus);

         void timeNodeCallback(Engine::Execution::TimeNodeElement* tn, ossia::time_value date);

    private:
        std::unordered_map<Id<Scenario::ConstraintModel>, ConstraintElement*> m_ossia_constraints;
        std::unordered_map<Id<Scenario::StateModel>, StateElement*> m_ossia_states;
        std::unordered_map<Id<Scenario::TimeNodeModel>, TimeNodeElement*> m_ossia_timenodes;
        std::unordered_map<Id<Scenario::EventModel>, EventElement*> m_ossia_timeevents;

        std::unordered_map<Id<Scenario::ConstraintModel>, Scenario::ConstraintModel*> m_executingConstraints;

        const Context& m_ctx;

        Scenario::CSPCoherencyCheckerInterface* m_checker{};
        QVector<Id<Scenario::TimeNodeModel>> m_pastTn{};
        Scenario::ElementsProperties m_properties{};
};

using ScenarioComponentFactory = ::Engine::Execution::ProcessComponentFactory_T<ScenarioComponent>;

}
}
