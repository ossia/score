#pragma once
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <iscore/document/DocumentContext.hpp>
#include <iscore/tools/std/Algorithms.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>

template<
        typename Component_T,
        typename System_T,
        typename ConstraintComponent_T,
        typename EventComponent_T,
        typename TimeNodeComponent_T,
        typename StateComponent_T>
class ScenarioComponentHierarchyManager : public Nano::Observer
{
    public:
        ScenarioComponentHierarchyManager(
                Component_T& component,
                Scenario::ScenarioModel& scenar,
                const System_T& doc,
                const iscore::DocumentContext& ctx,
                QObject* parentcomp
                ):
            m_scenario{scenar},
            m_component{component},
            m_system{doc},
            m_context{ctx},
            m_parentObject{parentcomp}
        {
            setup<ConstraintModel>();
            setup<EventModel>();
            setup<TimeNodeModel>();
            setup<StateModel>();
        }

        ~ScenarioComponentHierarchyManager()
        {
            for(auto element : m_constraints)
                cleanup(element);
            for(auto element : m_events)
                cleanup(element);
            for(auto element : m_states)
                cleanup(element);
            for(auto element : m_timeNodes)
                cleanup(element);
        }

    private:
        struct ConstraintPair {
                using element_t = ConstraintModel;
                ConstraintModel& element;
                ConstraintComponent_T& component;
        };
        struct EventPair {
                using element_t = EventModel;
                EventModel& element;
                EventComponent_T& component;
        };
        struct TimeNodePair {
                using element_t = TimeNodeModel;
                TimeNodeModel& element;
                TimeNodeComponent_T& component;
        };
        struct StatePair {
                using element_t = StateModel;
                StateModel& element;
                StateComponent_T& component;
        };

        template<typename T, bool dummy = true>
        struct MatchingComponent;

        template<typename Pair_T>
        void cleanup(const Pair_T& pair)
        {
            m_component.removing(pair.element, pair.component);
            pair.element.components.remove(pair.component);
        }

        template<typename elt_t>
        void setup()
        {
            using map_t = MatchingComponent<elt_t, true>;
            auto& member = m_scenario.*map_t::scenario_container;

            for(auto& elt : member)
            {
                add(elt);
            }

            member.mutable_added.template connect<
                    ScenarioComponentHierarchyManager,
                    &ScenarioComponentHierarchyManager::add>(this);

            member.removing.template connect<
                    ScenarioComponentHierarchyManager,
                    &ScenarioComponentHierarchyManager::remove>(this);
        }

        template<typename elt_t>
        void add(elt_t& element)
        {
            using map_t = MatchingComponent<elt_t, true>;
            auto comp = m_component.template make<elt_t, typename map_t::type>(
                            getStrongId(element.components),
                            element,
                            m_system,
                            m_context,
                            m_parentObject);
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


        Scenario::ScenarioModel& m_scenario;
        Component_T& m_component;

        const System_T& m_system;
        const iscore::DocumentContext& m_context;
        QObject* m_parentObject{};


        std::list<ConstraintPair> m_constraints;
        std::list<EventPair> m_events;
        std::list<TimeNodePair> m_timeNodes;
        std::list<StatePair> m_states;

        template<bool dummy>
        struct MatchingComponent<ConstraintModel, dummy> {
                using type = ConstraintComponent_T;
                using pair_type = ConstraintPair;
                static const constexpr auto local_container = &ScenarioComponentHierarchyManager::m_constraints;
                static const constexpr auto scenario_container = &Scenario::ScenarioModel::constraints;
        };
        template<bool dummy>
        struct MatchingComponent<EventModel, dummy> {
                using type = EventComponent_T;
                using pair_type = EventPair;
                static const constexpr auto local_container = &ScenarioComponentHierarchyManager::m_events;
                static const constexpr auto scenario_container = &Scenario::ScenarioModel::events;
        };
        template<bool dummy>
        struct MatchingComponent<TimeNodeModel, dummy> {
                using type = TimeNodeComponent_T;
                using pair_type = TimeNodePair;
                static const constexpr auto local_container = &ScenarioComponentHierarchyManager::m_timeNodes;
                static const constexpr auto scenario_container = &Scenario::ScenarioModel::timeNodes;
        };
        template<bool dummy>
        struct MatchingComponent<StateModel, dummy> {
                using type = StateComponent_T;
                using pair_type = StatePair;
                static const constexpr auto local_container = &ScenarioComponentHierarchyManager::m_states;
                static const constexpr auto scenario_container = &Scenario::ScenarioModel::states;
        };
};

