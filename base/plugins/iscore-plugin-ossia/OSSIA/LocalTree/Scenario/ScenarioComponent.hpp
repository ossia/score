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
           using system_t = Ossia::LocalTree::DocumentPlugin;
       ScenarioComponentBase(
               const Id<Component>& id,
               OSSIA::Node& parent,
               Scenario::ProcessModel& scenario,
               system_t& doc,
               QObject* parent_obj);

       template<typename Component_T, typename Element>
       Component_T* make(
               const Id<Component>& id,
               Element& elt,
               QObject* parent);


        void removing(
                const Scenario::ConstraintModel& elt,
                const ConstraintComponent& comp);

        void removing(
                const Scenario::EventModel& elt,
                const EventComponent& comp);

        void removing(
                const Scenario::TimeNodeModel& elt,
                const TimeNodeComponent& comp);

        void removing(
                const Scenario::StateModel& elt,
                const StateComponent& comp);

    private:
        system_t& m_sys;
        std::shared_ptr<OSSIA::Node> m_constraintsNode;
        std::shared_ptr<OSSIA::Node> m_eventsNode;
        std::shared_ptr<OSSIA::Node> m_timeNodesNode;
        std::shared_ptr<OSSIA::Node> m_statesNode;

        std::vector<std::unique_ptr<BaseProperty>> m_properties;
};

using ScenarioComponent = HierarchicalScenarioComponent<
    ScenarioComponentBase,
    ScenarioComponentBase::system_t,
    Scenario::ProcessModel,
    ConstraintComponent,
    EventComponent,
    TimeNodeComponent,
    StateComponent>;
}
}
