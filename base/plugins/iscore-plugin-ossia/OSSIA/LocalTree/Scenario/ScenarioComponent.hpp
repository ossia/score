#pragma once
#include <OSSIA/LocalTree/Scenario/ConstraintComponent.hpp>
#include <OSSIA/LocalTree/Scenario/EventComponent.hpp>
#include <OSSIA/LocalTree/Scenario/TimeNodeComponent.hpp>
#include <OSSIA/LocalTree/Scenario/StateComponent.hpp>
#include <Scenario/Document/Components/ScenarioComponent.hpp>

namespace Ossia
{
namespace LocalTree
{
class ScenarioComponentBase :
        public ProcessComponent_T<Scenario::ProcessModel>
{
       COMPONENT_METADATA("57c37324-f5a5-494e-8b45-206750d9fa77")

    public:
       ScenarioComponentBase(
               const Id<iscore::Component>& id,
               ossia::net::node_base& parent,
               Scenario::ProcessModel& scenario,
               DocumentPlugin& doc,
               QObject* parent_obj);

       template<typename Component_T, typename Element>
       Component_T* make(
               const Id<iscore::Component>& id,
               Element& elt);

        void removing(
                const Scenario::ConstraintModel& elt,
                const Constraint& comp);

        void removing(
                const Scenario::EventModel& elt,
                const Event& comp);

        void removing(
                const Scenario::TimeNodeModel& elt,
                const TimeNode& comp);

        void removing(
                const Scenario::StateModel& elt,
                const State& comp);

    private:
        ossia::net::node_base& m_constraintsNode;
        ossia::net::node_base& m_eventsNode;
        ossia::net::node_base& m_timeNodesNode;
        ossia::net::node_base& m_statesNode;

        std::vector<std::unique_ptr<BaseProperty>> m_properties;
};

using ScenarioComponent = HierarchicalScenarioComponent<
    ScenarioComponentBase,
    Scenario::ProcessModel,
    Constraint,
    Event,
    TimeNode,
    State>;

using ScenarioComponentFactory = Ossia::LocalTree::ProcessComponentFactory_T<ScenarioComponent>;
}
}
