#pragma once
#include <OSSIA/LocalTree/ConstraintComponent.hpp>
#include <OSSIA/LocalTree/EventComponent.hpp>
#include <OSSIA/LocalTree/TimeNodeComponent.hpp>
#include <OSSIA/LocalTree/StateComponent.hpp>
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

/*
        ConstraintComponent* makeConstraint(
                const Id<Component>& id,
                ConstraintModel& elt,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent);


        EventComponent* makeEvent(
                const Id<Component>& id,
                EventModel& elt,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent);

        TimeNodeComponent* makeTimeNode(
                const Id<Component>& id,
                TimeNodeModel& elt,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent);

        StateComponent* makeState(
                const Id<Component>& id,
                StateModel& elt,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent);

                */
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

class ScenarioComponentFactory final :
        public ProcessComponentFactory
{
    public:
        const factory_key_type& key_impl() const override
        {
            static const factory_key_type name{"ScenarioComponentFactory"};
            return name;
        }

        bool matches(
                Process& p,
                const OSSIA::LocalTree::DocumentPlugin&,
                const iscore::DocumentContext&) const override
        {
            return dynamic_cast<Scenario::ScenarioModel*>(&p);
        }

        // ProcessComponentFactory interface
    public:
        ProcessComponent* make(
                const Id<iscore::Component>& id,
                OSSIA::Node& parent,
                Process& proc,
                const DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt) const override
        {
            return new ScenarioComponent(id, parent, static_cast<Scenario::ScenarioModel&>(proc), doc, ctx, paren_objt);
        }
};
}
}
