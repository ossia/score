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
class Component : public IdentifiedObject<Component>
{
    public:
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
class ComponentFactory;

template<typename System_T, typename Component_T>
using ComponentFactoryKey =  StringKey<ComponentFactory<System_T, Component_T>>;

template<typename System_T, typename Component_T>
class ComponentFactory :
        public GenericFactoryInterface<ComponentFactoryKey<System_T, Component_T>>
{
    public:
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

        using factory_key_type = ComponentFactoryKey<System_T, Component_T>;

        virtual bool matches(
                Process&,
                const System_T&,
                const iscore::DocumentContext&) const;
};


template<typename System_T, typename Component_T>
class ComponentFactoryList :
        public iscore::FactoryListInterface
{
        using FactoryType = ComponentFactory<System_T, Component_T>;

        GenericFactoryMap_T<FactoryType, typename FactoryType::factory_key_type> m_list;

    public:
        static const iscore::FactoryBaseKey& staticFactoryKey() {
            return FactoryType::staticFactoryKey();
        }

        iscore::FactoryBaseKey name() const final override {
            return FactoryType::staticFactoryKey();
        }

        void insert(std::unique_ptr<iscore::FactoryInterfaceBase> e) final override
        {
            if(auto pf = dynamic_unique_ptr_cast<FactoryType>(std::move(e)))
                m_list.inscribe(std::move(pf));
        }

        const auto& list() const
        { return m_list; }

        FactoryType* factory(
                Process&,
                const iscore::DocumentPluginModel&,
                const iscore::DocumentContext&) const
        {
            return nullptr;
        }

};

template<
        typename System_T,
        typename Component_T>
class ConstraintComponent
{
    private:
        using ComponentFactory_t = ComponentFactoryList<System_T, Component_T>;
        const ComponentFactory_t& m_componentFactory;

        struct ProcessPair {
                Process& process;
                Component_T& component;
        };

    public:
        template<typename ComponentFactory_Fun>
        ConstraintComponent(
                ConstraintModel& constraint,
                const iscore::DocumentPluginModel& doc,
                const iscore::DocumentContext& ctx,
                ComponentFactory_Fun fun):
            m_componentFactory{ctx.app.components.factory<ComponentFactory_t>()}
        {
            auto processFun = [&] (Process& process) {
                if(auto factory = m_componentFactory.factory(process, doc, ctx))
                {
                    auto proc_comp = fun(*factory, process, doc, ctx);
                    process.components.add(proc_comp);
                    m_children.emplace_back(proc_comp);
                }
            };

            for(auto& process : constraint.processes)
            {
                processFun(process);
            }

            constraint.processes.mutable_added.connect(processFun);
            constraint.components.add(*this); // TODO in scenario instead maybe ?
        }

        ~ConstraintComponent()
        {
            for(ProcessPair element : m_children)
                element.process.components.remove(element.component);
        }

    private:
        std::vector<ProcessPair> m_children;
};

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
namespace OSSIA
{
namespace LocalTree
{
class ProcessComponent : public Component
{
};

class ProcessComponentFactory :
        public ::ComponentFactory<LocalTree::DocumentPlugin, LocalTree::TreeComponent>
{
    public:
        virtual Component* make(
                OSSIA::Node& parent,
                Process& proc,
                const iscore::DocumentPluginModel& doc,
                const iscore::DocumentContext& ctx) const = 0;
};

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

class ScenarioComponent : public ProcessComponent
{
        COMPONENT_METADATA(OSSIA::LocalTree::ScenarioComponent)
};

class ConstraintComponent : public Component
{
    public:
        using parent_t = ::ConstraintComponent<LocalTree::DocumentPlugin, LocalTree::TreeComponent>;

        ConstraintComponent(
                OSSIA::Node& parent,
                ConstraintModel& constraint,
                const iscore::DocumentPluginModel& doc,
                const iscore::DocumentContext& ctx):
            Component{{}, "", nullptr},
            m_thisNode{add_node(parent, constraint.metadata.name().toStdString())},
            m_processesNode{add_node(*m_thisNode, "processes")},
            m_baseComponent{constraint, doc, ctx,
                            [&] (
                            const ProcessComponentFactory& factory,
                            Process& process,
                            const iscore::DocumentPluginModel& doc,
                            const iscore::DocumentContext& ctx
                            ) {
            //auto it = add_node(*m_processesNode, process.metadata.name().toStdString());
            return factory.make(*m_processesNode, process, doc, ctx);
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
