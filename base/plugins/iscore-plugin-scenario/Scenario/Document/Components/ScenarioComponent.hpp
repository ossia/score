#pragma once

// Structure qui crée des objets qui se créent récursivement à l'insertion d'enfants
// dans le scénario.
// Doivent être créés après insertion, et supprimés avant suppression.
// Faire init de l'existant, et connection.
// Création initiale se fait par document plugin, ne sont pas sauvés / rechargés.

// Pattern général de création, puis delegate.

// Suppression : avoir connection dans deux sens .
// - si le composant parent est supprimé
// - si l'objet (constraint model) est supprimé
// - graphe de dépendances entre composants ??

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

#define COMPONENT_METADATA(TheType) \
    public: \
    static const Component::Key& static_key() { \
      static const Component::Key& s \
      { #TheType }; \
      return s; \
    } \
    \
    const Component::Key& key() const final override { \
      return static_key(); \
    } \
    private:


class Component : public IdentifiedObject<Component>
{
    public:
        using IdentifiedObject<Component>::IdentifiedObject;
        using Key = StringKey<Component>;
        virtual const Key& key() const = 0;

        virtual ~Component()
        {

        }

};

class ComponentContainer
{
    public:
        void add(Component*)
        {

        }

        void remove()
        {
            // Est-ce qu'on veut avoir plusieurs
            // composants du même type ? bouef...

            // Les composants doivent maintenir un tableau de leurs composants enfants.
            // Comment ? Trouver un type map meilleur que ce qu'il y a dans OSSIADocumentPlugin.
        }

        ~ComponentContainer()
        {

        }

    private:
        std::vector<Component*> m_components;
};

template<typename System_T, typename Component_T>
class GenericProcessComponentFactory;

template<typename System_T, typename Component_T>
using ComponentFactoryKey =  StringKey<GenericProcessComponentFactory<System_T, Component_T>>;

template<typename System_T, typename Component_T>
class GenericProcessComponentFactory :
        public GenericFactoryInterface<ComponentFactoryKey<System_T, Component_T>>
{
    public:
        using factory_key_type = ComponentFactoryKey<System_T, Component_T>;

        static const iscore::FactoryBaseKey& staticFactoryKey() {
            static const iscore::FactoryBaseKey s{
                "ComponentFactory<" +
                System_T::className +
                Component_T::className + ">"
            };
            return s;
        }

        const iscore::FactoryBaseKey& factoryKey() const final override {
            return staticFactoryKey();
        }

        virtual bool matches(
                Process&,
                const System_T&,
                const iscore::DocumentContext&) const = 0;
};


template<
        typename System_T,
        typename Component_T,
        typename Factory_T>
class GenericProcessComponentFactoryList :
        public iscore::FactoryListInterface
{
        std::vector<std::unique_ptr<Factory_T>> m_list;

    public:
        using factory_t = Factory_T;
        static const iscore::FactoryBaseKey& staticFactoryKey() {
            return Factory_T::staticFactoryKey();
        }

        iscore::FactoryBaseKey name() const final override {
            return Factory_T::staticFactoryKey();
        }

        void insert(std::unique_ptr<iscore::FactoryInterfaceBase> e) final override
        {
            if(auto pf = dynamic_unique_ptr_cast<Factory_T>(std::move(e)))
                m_list.push_back(std::move(pf));
        }

        const auto& list() const
        { return m_list; }

        Factory_T* factory(
                Process& proc,
                const System_T& doc,
                const iscore::DocumentContext& ctx) const
        {
            for(auto& factory : list())
            {
                if(factory->matches(proc, doc, ctx))
                {
                    return factory.get();
                }
            }

            return nullptr;
        }
};

template<
        typename Component_T,
        typename System_T,
        typename ProcessComponent_T,
        typename ProcessComponentFactoryList_T>
class ConstraintComponentHierarchyManager : public Nano::Observer
{
    private:
        struct ProcessPair {
                Process& process;
                ProcessComponent_T& component;
        };

    public:
        ConstraintComponentHierarchyManager(
                Component_T& component,
                ConstraintModel& cst,
                const System_T& doc,
                const iscore::DocumentContext& ctx,
                QObject* component_as_parent):
            m_constraint{cst},
            m_component{component},
            m_componentFactory{ctx.app.components.factory<ProcessComponentFactoryList_T>()},
            m_system{doc},
            m_context{ctx},
            m_parentObject{component_as_parent}
        {
            for(auto& process : m_constraint.processes)
            {
                add(process);
            }

            m_constraint.processes.mutable_added.connect<
                    ConstraintComponentHierarchyManager,
                    &ConstraintComponentHierarchyManager::add>(this);

            m_constraint.processes.removing.connect<
                    ConstraintComponentHierarchyManager,
                    &ConstraintComponentHierarchyManager::remove>(this);
        }

        void add(Process& process)
        {
            // Will return a factory for the given process if available
            if(auto factory = m_componentFactory.factory(process, m_system, m_context))
            {
                // The subclass should provide this function to construct
                // the correct component relative to this process.
                auto proc_comp = m_component.make_processComponent(
                                     Id<Component>{}/*getStrongId(constraint.components)*/,
                                     *factory, process, m_system, m_context, m_parentObject);
                if(proc_comp)
                {
                    // process.components.add(proc_comp);
                    m_children.emplace_back(ProcessPair{process, *proc_comp});
                }
            }
        }

        void remove(const Process& process)
        {
            auto it = find_if(m_children, [&] (auto pair) {
                return &pair.process == &process;
            });

            if(it != m_children.end())
            {
                remove(*it);
                m_children.erase(it);
            }
        }

        void remove(const ProcessPair& pair)
        {
            //pair.process.components.remove(pair.component);
            delete &pair.component;
        }

        virtual ~ConstraintComponentHierarchyManager()
        {
            for(ProcessPair element : m_children)
            {
                remove(element);
            }
        }

    private:
        ConstraintModel& m_constraint;
        Component_T& m_component;
        const ProcessComponentFactoryList_T& m_componentFactory;
        const System_T& m_system;
        const iscore::DocumentContext& m_context;

        QObject* m_parentObject{};

        std::list<ProcessPair> m_children; // todo map ? multi_index with both index of the component and of the process ?
};



template<
        typename Component_T,
        typename System_T,
        typename ConstraintComponent_T,
        typename EventComponent_T,
        typename TimeNodeComponent_T,
        typename StateComponent_T>
class ScenarioComponentHierarchyManager : public Nano::Observer
{
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

    public:
        template<typename Pair_T>
        void remove(const Pair_T& pair)
        {
            m_component.removing(pair.element, pair.component);
            //pair.process.components.remove(pair.component);
            delete &pair.component;
        }


        template<typename ElementPairType, typename ElementContainer>
        void setup(ElementContainer member_ptr)
        {
            auto& member = m_scenario.*member_ptr;
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

        void add(ConstraintModel& element)
        {
            auto comp = m_component.makeConstraint(
                            Id<Component>{}/*getStrongId(scenario.components)*/,
                            element,
                            m_system,
                            m_context,
                            m_parentObject);
            if(comp)
            {
                // constraint.components.add(cst_comp);
                m_constraints.emplace_back(ConstraintPair{element, *comp});
            }
        }

        void add(EventModel& element)
        {
            auto comp = m_component.makeEvent(
                            Id<Component>{}/*getStrongId(scenario.components)*/,
                            element,
                            m_system,
                            m_context,
                            m_parentObject);
            if(comp)
            {
                // constraint.components.add(cst_comp);
                m_events.emplace_back(EventPair{element, *comp});
            }
        }

        void add(TimeNodeModel& element)
        {
            auto comp = m_component.makeTimeNode(
                            Id<Component>{}/*getStrongId(scenario.components)*/,
                            element,
                            m_system,
                            m_context,
                            m_parentObject);
            if(comp)
            {
                // constraint.components.add(cst_comp);
                m_timeNodes.emplace_back(TimeNodePair{element, *comp});
            }
        }

        void add(StateModel& element)
        {
            auto comp = m_component.makeState(
                            Id<Component>{}/*getStrongId(scenario.components)*/,
                            element,
                            m_system,
                            m_context,
                            m_parentObject);
            if(comp)
            {
                // constraint.components.add(cst_comp);
                m_states.emplace_back(StatePair{element, *comp});
            }
        }


        void remove(const ConstraintModel& element)
        {
            auto it = find_if(m_constraints, [&] (auto pair) {
                return &pair.element == &element;
            });

            if(it != m_constraints.end())
            {
                remove(*it);
                m_constraints.erase(it);
            }
        }

        void remove(const EventModel& element)
        {
            auto it = find_if(m_events, [&] (auto pair) {
                return &pair.element == &element;
            });

            if(it != m_events.end())
            {
                remove(*it);
                m_events.erase(it);
            }
        }

        void remove(const TimeNodeModel& element)
        {
            auto it = find_if(m_timeNodes, [&] (auto pair) {
                return &pair.element == &element;
            });

            if(it != m_timeNodes.end())
            {
                remove(*it);
                m_timeNodes.erase(it);
            }
        }

        void remove(const StateModel& element)
        {
            auto it = find_if(m_states, [&] (auto pair) {
                return &pair.element == &element;
            });

            if(it != m_states.end())
            {
                remove(*it);
                m_states.erase(it);
            }
        }

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
            setup<ConstraintPair>(&Scenario::ScenarioModel::constraints);
            setup<EventPair>(&Scenario::ScenarioModel::events);
            setup<TimeNodePair>(&Scenario::ScenarioModel::timeNodes);
            setup<StatePair>(&Scenario::ScenarioModel::states);
        }

        ~ScenarioComponentHierarchyManager()
        {
            for(auto element : m_constraints)
                remove(element);
            for(auto element : m_events)
                remove(element);
            for(auto element : m_states)
                remove(element);
            for(auto element : m_timeNodes)
                remove(element);
        }


    private:
        Scenario::ScenarioModel& m_scenario;
        Component_T& m_component;

        const System_T& m_system;
        const iscore::DocumentContext& m_context;
        QObject* m_parentObject{};


        std::list<ConstraintPair> m_constraints;
        std::list<EventPair> m_events;
        std::list<TimeNodePair> m_timeNodes;
        std::list<StatePair> m_states;
};

