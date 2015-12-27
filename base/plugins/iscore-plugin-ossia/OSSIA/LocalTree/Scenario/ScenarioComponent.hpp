#pragma once
#include <OSSIA/LocalTree/Scenario/ConstraintComponent.hpp>
#include <OSSIA/LocalTree/Scenario/EventComponent.hpp>
#include <OSSIA/LocalTree/Scenario/TimeNodeComponent.hpp>
#include <OSSIA/LocalTree/Scenario/StateComponent.hpp>
#include <Scenario/Document/Components/ScenarioComponent.hpp>

namespace OSSIA
{
namespace LocalTree
{
class ScenarioComponent : public ProcessComponent
{
       COMPONENT_METADATA(OSSIA::LocalTree::ScenarioComponent)

        using system_t = OSSIA::LocalTree::DocumentPlugin;
        using hierarchy_t =
           ScenarioComponentHierarchyManager<
               ScenarioComponent,
               system_t,
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
               const system_t& doc,
               const iscore::DocumentContext& ctx,
               QObject* parent_obj);

       template<typename Element, typename Component_T>
       Component_T* make(
               const Id<Component>& id,
               Element& elt,
               const system_t& doc,
               const iscore::DocumentContext& ctx,
               QObject* parent);

        void removing(
                const ConstraintModel& elt,
                const ConstraintComponent& comp);

        void removing(
                const EventModel& elt,
                const EventComponent& comp);

        void removing(
                const TimeNodeModel& elt,
                const TimeNodeComponent& comp);

        void removing(
                const StateModel& elt,
                const StateComponent& comp);

    private:
        std::shared_ptr<OSSIA::Node> m_constraintsNode;
        std::shared_ptr<OSSIA::Node> m_eventsNode;
        std::shared_ptr<OSSIA::Node> m_timeNodesNode;
        std::shared_ptr<OSSIA::Node> m_statesNode;

        std::vector<BaseProperty*> m_properties;

        hierarchy_t m_hm;

};

}
}
