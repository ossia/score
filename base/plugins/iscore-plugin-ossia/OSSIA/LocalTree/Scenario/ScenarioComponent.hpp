#pragma once
#include <OSSIA/LocalTree/Scenario/ConstraintComponent.hpp>
#include <OSSIA/LocalTree/Scenario/EventComponent.hpp>
#include <OSSIA/LocalTree/Scenario/TimeNodeComponent.hpp>
#include <OSSIA/LocalTree/Scenario/StateComponent.hpp>
#include <Scenario/Document/Components/ScenarioComponent.hpp>

template<
        typename Component_T,
        typename System_T,
        typename Scenario_T,
        typename ConstraintComponent_T,
        typename EventComponent_T,
        typename TimeNodeComponent_T,
        typename StateComponent_T>
class HierarchicalScenarioComponent :
        public Component_T,
        public Nano::Observer
{
    public:
        struct ConstraintPair {
                using element_t = Scenario::ConstraintModel;
                Scenario::ConstraintModel& element;
                ConstraintComponent_T& component;
        };
        struct EventPair {
                using element_t = Scenario::EventModel;
                Scenario::EventModel& element;
                EventComponent_T& component;
        };
        struct TimeNodePair {
                using element_t = Scenario::TimeNodeModel;
                Scenario::TimeNodeModel& element;
                TimeNodeComponent_T& component;
        };
        struct StatePair {
                using element_t = Scenario::StateModel;
                Scenario::StateModel& element;
                StateComponent_T& component;
        };


        template<typename... Args>
        HierarchicalScenarioComponent(Args&&... args):
            Component_T{std::forward<Args>(args)...}
        {
            setup<Scenario::ConstraintModel>();
            setup<Scenario::EventModel>();
            setup<Scenario::TimeNodeModel>();
            setup<Scenario::StateModel>();
        }

        const std::list<ConstraintPair>& constraints() const
        { return m_constraints; }
        const std::list<EventPair>& events() const
        { return m_events; }
        const std::list<StatePair>& states() const
        { return m_states; }
        const std::list<TimeNodePair>& timeNodes() const
        { return m_timeNodes; }

        void clear()
        {
            for(auto element : m_constraints)
                cleanup(element);
            for(auto element : m_events)
                cleanup(element);
            for(auto element : m_states)
                cleanup(element);
            for(auto element : m_timeNodes)
                cleanup(element);

            m_constraints.clear();
            m_events.clear();
            m_states.clear();
            m_timeNodes.clear();
        }

        ~HierarchicalScenarioComponent()
        {
            clear();
        }

    private:

        template<typename T, bool dummy = true>
        struct MatchingComponent;

        template<typename Pair_T>
        void cleanup(const Pair_T& pair)
        {
            Component_T::removing(pair.element, pair.component);
            pair.element.components.remove(pair.component);
        }

        template<typename elt_t>
        void setup()
        {
            using map_t = MatchingComponent<elt_t, true>;
            auto&& member = map_t::scenario_container(this->process());

            for(auto& elt : member)
            {
                add(elt);
            }

            member.mutable_added.template connect<
                    HierarchicalScenarioComponent,
                    &HierarchicalScenarioComponent::add>(this);

            member.removing.template connect<
                    HierarchicalScenarioComponent,
                    &HierarchicalScenarioComponent::remove>(this);
        }

        template<typename elt_t>
        void add(elt_t& element)
        {
            using map_t = MatchingComponent<elt_t, true>;
            auto comp = Component_T::template make<typename map_t::type>(
                            getStrongId(element.components),
                            element,
                            this);
            if(comp)
            {
                element.components.add(comp);
                (this->*map_t::local_container).emplace_back(typename map_t::pair_type{element, *comp});
            }
        }

        template<typename elt_t>
        void remove(const elt_t& element)
        {
            using map_t = MatchingComponent<elt_t, true>;
            auto& container = this->*map_t::local_container;

            auto it = find_if(container, [&] (auto pair) {
                return &pair.element == &element;
            });

            if(it != container.end())
            {
                cleanup(*it);
                container.erase(it);
            }
        }

        std::list<ConstraintPair> m_constraints;
        std::list<EventPair> m_events;
        std::list<TimeNodePair> m_timeNodes;
        std::list<StatePair> m_states;

        template<bool dummy>
        struct MatchingComponent<Scenario::ConstraintModel, dummy> {
                using type = ConstraintComponent_T;
                using pair_type = ConstraintPair;
                static const constexpr auto local_container = &HierarchicalScenarioComponent::m_constraints;
                static const constexpr auto scenario_container = Scenario::ScenarioElementTraits<Scenario_T, Scenario::ConstraintModel>::accessor;
        };
        template<bool dummy>
        struct MatchingComponent<Scenario::EventModel, dummy> {
                using type = EventComponent_T;
                using pair_type = EventPair;
                static const constexpr auto local_container = &HierarchicalScenarioComponent::m_events;
                static const constexpr auto scenario_container = Scenario::ScenarioElementTraits<Scenario_T, Scenario::EventModel>::accessor;
        };
        template<bool dummy>
        struct MatchingComponent<Scenario::TimeNodeModel, dummy> {
                using type = TimeNodeComponent_T;
                using pair_type = TimeNodePair;
                static const constexpr auto local_container = &HierarchicalScenarioComponent::m_timeNodes;
                static const constexpr auto scenario_container = Scenario::ScenarioElementTraits<Scenario_T, Scenario::TimeNodeModel>::accessor;
        };
        template<bool dummy>
        struct MatchingComponent<Scenario::StateModel, dummy> {
                using type = StateComponent_T;
                using pair_type = StatePair;
                static const constexpr auto local_container = &HierarchicalScenarioComponent::m_states;
                static const constexpr auto scenario_container = Scenario::ScenarioElementTraits<Scenario_T, Scenario::StateModel>::accessor;
        };
};

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

       ~ScenarioComponentBase();

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
