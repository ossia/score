#include "ScenarioTreeBuilder.hpp"

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
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>

#include "LocalTreeDocumentPlugin.hpp"

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
        typename System_T,
        typename Component_T,
        typename ComponentFactory_T>
class ConstraintComponent
{
    private:
        //using ComponentFactory_t = ComponentFactoryList<System_T, Component_T, Factory_T>;
        const ComponentFactory_T& m_componentFactory;

        struct ProcessPair {
                Process& process;
                Component_T& component;
        };

    public:
        template<typename ComponentFactory_Fun>
        ConstraintComponent(
                ConstraintModel& constraint,
                const System_T& doc,
                const iscore::DocumentContext& ctx,
                ComponentFactory_Fun fun):
            m_componentFactory{ctx.app.components.factory<ComponentFactory_T>()}
        {
            auto processFun = [&] (Process& process) {
                // Will return a factory for the given process if available
                if(auto factory = m_componentFactory.factory(process, doc, ctx))
                {
                    // The subclass should provide this function to construct
                    // the correct component relative to this process.
                    auto proc_comp = fun(*factory, process, doc, ctx);
                    if(proc_comp)
                    {
                        // process.components.add(proc_comp);
                        m_children.emplace_back(ProcessPair{process, *proc_comp});
                    }
                }
            };

            for(auto& process : constraint.processes)
            {
                processFun(process);
            }

            constraint.processes.mutable_added.connect(processFun);
            // constraint.components.add(*this); // TODO in scenario instead maybe ?
        }

        ~ConstraintComponent()
        {
            for(ProcessPair element : m_children)
            {
                // element.process.components.remove(element.component);
            }
        }

    private:
        std::vector<ProcessPair> m_children;
};

/*
void OSSIA::LocalTree::ScenarioVisitor::visit(
        ModelMetadata& metadata,
        const std::shared_ptr<OSSIA::Node>& parent)
{
    add_getProperty<QString>(*parent, "name", &metadata,
                             &ModelMetadata::name,
                             &ModelMetadata::nameChanged);
    add_property<QString>(*parent, "comment", &metadata,
                          &ModelMetadata::comment,
                          &ModelMetadata::setComment,
                          &ModelMetadata::commentChanged);
    add_property<QString>(*parent, "label", &metadata,
                          &ModelMetadata::label,
                          &ModelMetadata::setLabel,
                          &ModelMetadata::labelChanged);
}

void OSSIA::LocalTree::ScenarioVisitor::visit(
        ConstraintModel& constraint,
        const std::shared_ptr<OSSIA::Node>& parent)
{
}
*/
namespace OSSIA
{
namespace LocalTree
{
class ProcessComponent : public Component
{
        ISCORE_METADATA(OSSIA::LocalTree::ProcessComponent)
};

class ProcessComponentFactory :
        public ::GenericProcessComponentFactory<
            LocalTree::DocumentPlugin,
            LocalTree::ProcessComponent>
{
    public:
        virtual ProcessComponent* make(
                OSSIA::Node& parent,
                Process& proc,
                const LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx) const = 0;
};

class ProcessComponentFactoryList :
        public ::GenericProcessComponentFactoryList<
            LocalTree::DocumentPlugin,
            LocalTree::ProcessComponent,
            LocalTree::ProcessComponentFactory>
{
    public:
        /*
        ProcessComponent* make(
                OSSIA::Node& parent,
                Process& proc,
                const LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx)
        {
            auto matching = factory(proc, doc, ctx);
            if(matching)
            {
                return matching->make(parent, proc, doc, ctx);
            }

            return nullptr;
        }
        */
};

class ScenarioComponent : public ProcessComponent
{
        COMPONENT_METADATA(OSSIA::LocalTree::ScenarioComponent)
};

class ConstraintComponent : public Component
{
    public:
        using system_t = OSSIA::LocalTree::DocumentPlugin;
        using process_component_t = OSSIA::LocalTree::ProcessComponent;
        using process_component_factory_t = OSSIA::LocalTree::ProcessComponentFactory;
        using process_component_factory_list_t = OSSIA::LocalTree::ProcessComponentFactoryList;

        using parent_t = ::ConstraintComponent<
            system_t,
            process_component_t,
            process_component_factory_list_t
        >;

        ConstraintComponent(
                OSSIA::Node& parent,
                ConstraintModel& constraint,
                const system_t& doc,
                const iscore::DocumentContext& ctx):
            Component{Id<Component>{}, "", nullptr},
            m_thisNode{add_node(parent, constraint.metadata.name().toStdString())},
            m_processesNode{add_node(*m_thisNode, "processes")},
            m_baseComponent{constraint, doc, ctx,
                            [&](const process_component_factory_t& factory,
                                Process& process,
                                const system_t& doc,
                                const iscore::DocumentContext& ctx) {
            return factory.make(*m_processesNode, process, doc, ctx); //auto it = add_node(*m_processesNode, process.metadata.name().toStdString());
        }}
        {
            using tv_t = ::TimeValue;

            //visit(constraint.metadata, m_thisNode);
            add_property<float>(*m_thisNode, "yPos", &constraint,
                                &ConstraintModel::heightPercentage,
                                &ConstraintModel::setHeightPercentage,
                                &ConstraintModel::heightPercentageChanged);

            add_getProperty<tv_t>(*m_thisNode, "min", &constraint.duration,
                                  &ConstraintDurations::minDuration,
                                  &ConstraintDurations::minDurationChanged
                                  );
            add_getProperty<tv_t>(*m_thisNode, "max", &constraint.duration,
                                  &ConstraintDurations::maxDuration,
                                  &ConstraintDurations::maxDurationChanged
                                  );
            add_getProperty<tv_t>(*m_thisNode, "default", &constraint.duration,
                                  &ConstraintDurations::defaultDuration,
                                  &ConstraintDurations::defaultDurationChanged
                                  );
            add_getProperty<float>(*m_thisNode, "play", &constraint.duration,
                                   &ConstraintDurations::playPercentage,
                                   &ConstraintDurations::playPercentageChanged
                                   );
        }

    private:
        std::shared_ptr<OSSIA::Node> m_thisNode;
        std::shared_ptr<OSSIA::Node> m_processesNode;
        parent_t m_baseComponent;
};
}
}
struct ScenarioTreeBuilder
{
         iscore::DocumentPluginModel& m_doc;

        void build(
                Scenario::ScenarioModel& scenario,
                const iscore::DocumentContext& ctx)
        {

            for(auto& constraint : scenario.constraints)
            {

            }
        }

        void build(
                ConstraintModel& constraint,
                const iscore::DocumentContext& ctx)
        {

        }
};
