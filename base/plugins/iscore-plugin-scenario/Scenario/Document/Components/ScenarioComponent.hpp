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
        typename Scenario_T,
        typename ConstraintComponent_T,
        typename EventComponent_T,
        typename TimeNodeComponent_T,
        typename StateComponent_T>
class ScenarioComponentHierarchyManager : public Nano::Observer
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

        Scenario_T& scenario;

        ScenarioComponentHierarchyManager(
                Component_T& component,
                Scenario_T& scenar,
                const System_T& doc,
                const iscore::DocumentContext& ctx,
                QObject* parentcomp
                ):
            scenario{scenar},
            m_component{component},
            m_system{doc},
            m_context{ctx},
            m_parentObject{parentcomp}
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
            auto&& member = map_t::scenario_container(scenario);

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
            auto comp = m_component.template make<typename map_t::type>(
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


        Component_T& m_component;

        const System_T& m_system;
        const iscore::DocumentContext& m_context;
        QObject* m_parentObject{};


        std::list<ConstraintPair> m_constraints;
        std::list<EventPair> m_events;
        std::list<TimeNodePair> m_timeNodes;
        std::list<StatePair> m_states;

        template<bool dummy>
        struct MatchingComponent<Scenario::ConstraintModel, dummy> {
                using type = ConstraintComponent_T;
                using pair_type = ConstraintPair;
                static const constexpr auto local_container = &ScenarioComponentHierarchyManager::m_constraints;
                static const constexpr auto scenario_container = Scenario::ScenarioElementTraits<Scenario_T, Scenario::ConstraintModel>::accessor;
        };
        template<bool dummy>
        struct MatchingComponent<Scenario::EventModel, dummy> {
                using type = EventComponent_T;
                using pair_type = EventPair;
                static const constexpr auto local_container = &ScenarioComponentHierarchyManager::m_events;
                static const constexpr auto scenario_container = Scenario::ScenarioElementTraits<Scenario_T, Scenario::EventModel>::accessor;
        };
        template<bool dummy>
        struct MatchingComponent<Scenario::TimeNodeModel, dummy> {
                using type = TimeNodeComponent_T;
                using pair_type = TimeNodePair;
                static const constexpr auto local_container = &ScenarioComponentHierarchyManager::m_timeNodes;
                static const constexpr auto scenario_container = Scenario::ScenarioElementTraits<Scenario_T, Scenario::TimeNodeModel>::accessor;
        };
        template<bool dummy>
        struct MatchingComponent<Scenario::StateModel, dummy> {
                using type = StateComponent_T;
                using pair_type = StatePair;
                static const constexpr auto local_container = &ScenarioComponentHierarchyManager::m_states;
                static const constexpr auto scenario_container = Scenario::ScenarioElementTraits<Scenario_T, Scenario::StateModel>::accessor;
        };
};



template<
        typename Component_T,
        typename System_T,
        typename BaseScenario_T,
        typename ConstraintComponent_T,
        typename EventComponent_T,
        typename TimeNodeComponent_T,
        typename StateComponent_T>
class BaseScenarioComponentHierarchyManager : public Nano::Observer
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

        BaseScenario_T& scenario;

        BaseScenarioComponentHierarchyManager(
                Component_T& component,
                BaseScenario_T& scenar,
                const System_T& doc,
                const iscore::DocumentContext& ctx,
                QObject* parentcomp
                ):
            scenario{scenar},
            m_component{component},
            m_system{doc},
            m_context{ctx},
            m_parentObject{parentcomp},
            m_constraints{{setup<Scenario::ConstraintModel>(0)}},
            m_events{{setup<Scenario::EventModel>(0), setup<Scenario::EventModel>(1)}},
            m_timeNodes{{setup<Scenario::TimeNodeModel>(0), setup<Scenario::TimeNodeModel>(1)}},
            m_states{{setup<Scenario::StateModel>(0), setup<Scenario::StateModel>(1)}}
        {
        }

        const auto& constraints() const
        { return m_constraints; }
        const auto& events() const
        { return m_events; }
        const auto& states() const
        { return m_states; }
        const auto& timeNodes() const
        { return m_timeNodes; }

        ~BaseScenarioComponentHierarchyManager()
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

        template<typename T, bool dummy = true>
        struct MatchingComponent;

        template<typename Pair_T>
        void cleanup(const Pair_T& pair)
        {
            m_component.removing(pair.element, pair.component);
            pair.element.components.remove(pair.component);
        }

        template<typename elt_t>
        auto setup(int pos)
        {
            using map_t = MatchingComponent<elt_t, true>;
            auto&& member = map_t::scenario_container(scenario);

            return make(member[pos], pos);

        }

        template<typename elt_t>
        auto make(elt_t& element, int pos)
        {
            using map_t = MatchingComponent<elt_t, true>;
            auto comp = m_component.template make<typename map_t::type>(
                            getStrongId(element.components),
                            element,
                            m_system,
                            m_context,
                            m_parentObject);

            ISCORE_ASSERT(comp);
            element.components.add(comp);
            return typename map_t::pair_type{element, *comp};
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


        Component_T& m_component;

        const System_T& m_system;
        const iscore::DocumentContext& m_context;
        QObject* m_parentObject{};


        std::array<ConstraintPair, 1> m_constraints;
        std::array<EventPair, 2> m_events;
        std::array<TimeNodePair, 2> m_timeNodes;
        std::array<StatePair, 2> m_states;

        template<bool dummy>
        struct MatchingComponent<Scenario::ConstraintModel, dummy> {
                using type = ConstraintComponent_T;
                using pair_type = ConstraintPair;
                static const constexpr auto local_container = &BaseScenarioComponentHierarchyManager::m_constraints;
                static const constexpr auto scenario_container = Scenario::ScenarioElementTraits<BaseScenario_T, Scenario::ConstraintModel>::accessor;
        };
        template<bool dummy>
        struct MatchingComponent<Scenario::EventModel, dummy> {
                using type = EventComponent_T;
                using pair_type = EventPair;
                static const constexpr auto local_container = &BaseScenarioComponentHierarchyManager::m_events;
                static const constexpr auto scenario_container = Scenario::ScenarioElementTraits<BaseScenario_T, Scenario::EventModel>::accessor;
        };
        template<bool dummy>
        struct MatchingComponent<Scenario::TimeNodeModel, dummy> {
                using type = TimeNodeComponent_T;
                using pair_type = TimeNodePair;
                static const constexpr auto local_container = &BaseScenarioComponentHierarchyManager::m_timeNodes;
                static const constexpr auto scenario_container = Scenario::ScenarioElementTraits<BaseScenario_T, Scenario::TimeNodeModel>::accessor;
        };
        template<bool dummy>
        struct MatchingComponent<Scenario::StateModel, dummy> {
                using type = StateComponent_T;
                using pair_type = StatePair;
                static const constexpr auto local_container = &BaseScenarioComponentHierarchyManager::m_states;
                static const constexpr auto scenario_container = Scenario::ScenarioElementTraits<BaseScenario_T, Scenario::StateModel>::accessor;
        };
};


