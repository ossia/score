#pragma once
#include <OSSIA/LocalTree/ConstraintComponent.hpp>

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
               QObject* parent_obj):
           ProcessComponent{id, "ScenarioComponent", parent_obj},
           m_thisNode{add_node(parent, scenario.metadata.name().toStdString())},
           m_constraintsNode{add_node(*m_thisNode, "constraints")},
           m_eventsNode{add_node(*m_thisNode, "events")},
           m_timeNodesNode{add_node(*m_thisNode, "timenodes")},
           m_statesNode{add_node(*m_thisNode, "states")},
           m_hm{*this, scenario, doc, ctx, this}
       {
           make_metadata_node(scenario.metadata, *m_thisNode);
       }


        ConstraintComponent* makeConstraint(
                const Id<Component>& id,
                ConstraintModel& elt,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent)
        {
            return new ConstraintComponent{*m_constraintsNode, id, elt, doc, ctx, parent};
        }

        EventComponent* makeEvent(
                const Id<Component>& id,
                EventModel& elt,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent)
        {
            return new EventComponent{*m_eventsNode, id, elt, doc, ctx, parent};
        }

        TimeNodeComponent* makeTimeNode(
                const Id<Component>& id,
                TimeNodeModel& elt,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent)
        {
            return new TimeNodeComponent{*m_timeNodesNode, id, elt, doc, ctx, parent};
        }

        StateComponent* makeState(
                const Id<Component>& id,
                StateModel& elt,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent)
        {
            return new StateComponent{*m_statesNode, id, elt, doc, ctx, parent};
        }

        void removing(
                const ConstraintModel& elt,
                const ConstraintComponent& comp)
        {
            auto it = find_if(m_constraintsNode->children(), [&] (const auto& node)
            { return node == comp.node(); });
            ISCORE_ASSERT(it != m_constraintsNode->children().end());

            m_constraintsNode->children().erase(it);
        }

        void removing(
                const EventModel& elt,
                const EventComponent& comp)
        {
            auto it = find_if(m_eventsNode->children(), [&] (const auto& node)
            { return node == comp.node(); });
            ISCORE_ASSERT(it != m_eventsNode->children().end());

            m_eventsNode->children().erase(it);
        }

        void removing(
                const TimeNodeModel& elt,
                const TimeNodeComponent& comp)
        {
            auto it = find_if(m_timeNodesNode->children(), [&] (const auto& node)
            { return node == comp.node(); });
            ISCORE_ASSERT(it != m_timeNodesNode->children().end());

            m_timeNodesNode->children().erase(it);
        }

        void removing(
                const StateModel& elt,
                const StateComponent& comp)
        {
            auto it = find_if(m_statesNode->children(), [&] (const auto& node)
            { return node == comp.node(); });
            ISCORE_ASSERT(it != m_statesNode->children().end());

            m_statesNode->children().erase(it);
        }

    private:
        std::shared_ptr<OSSIA::Node> m_thisNode;
        std::shared_ptr<OSSIA::Node> m_constraintsNode;
        std::shared_ptr<OSSIA::Node> m_eventsNode;
        std::shared_ptr<OSSIA::Node> m_timeNodesNode;
        std::shared_ptr<OSSIA::Node> m_statesNode;

        hierarchy_t m_hm;

};



}
}
