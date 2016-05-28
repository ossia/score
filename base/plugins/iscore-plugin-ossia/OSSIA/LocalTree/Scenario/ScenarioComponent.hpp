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
class ScenarioComponent final : public ProcessComponent
{
       COMPONENT_METADATA(Ossia::LocalTree::ScenarioComponent)

        using system_t = Ossia::LocalTree::DocumentPlugin;
        using hierarchy_t =
           ScenarioComponentHierarchyManager<
               ScenarioComponent,
               system_t,
               Scenario::ScenarioModel,
               ConstraintComponent,
               EventComponent,
               TimeNodeComponent,
               StateComponent
        >;

    public:
       ScenarioComponent(
               const Id<Component>& id,
               OSSIA::Node& parent,
               Scenario::ScenarioModel& scenario,
               system_t& doc,
               const iscore::DocumentContext& ctx,
               QObject* parent_obj);

       template<typename Component_T, typename Element>
       Component_T* make(
               const Id<Component>& id,
               Element& elt,
               system_t& doc,
               const iscore::DocumentContext& ctx,
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
        std::shared_ptr<OSSIA::Node> m_constraintsNode;
        std::shared_ptr<OSSIA::Node> m_eventsNode;
        std::shared_ptr<OSSIA::Node> m_timeNodesNode;
        std::shared_ptr<OSSIA::Node> m_statesNode;

        std::vector<std::unique_ptr<BaseProperty>> m_properties;

        hierarchy_t m_hm;

};

}
}
