#pragma once
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <iscore/component/Component.hpp>
#include <iscore/component/ComponentFactory.hpp>
#include <Scenario/Document/Components/ProcessComponent.hpp>
#include <OSSIA/LocalTree/LocalTreeComponent.hpp>

// TODO clean me up
namespace Ossia
{
namespace LocalTree
{
class ISCORE_PLUGIN_OSSIA_EXPORT ProcessComponent :
        public Component<Scenario::GenericProcessComponent<DocumentPlugin>>
{
    public:
        ProcessComponent(
                OSSIA::Node& node,
                Process::ProcessModel& proc,
                DocumentPlugin& doc,
                const Id<iscore::Component>& id,
                const QString& name,
                QObject* parent);

        virtual ~ProcessComponent();
};

template<typename Process_T>
using ProcessComponent_T = Scenario::GenericProcessComponent_T<ProcessComponent, Process_T>;

class ISCORE_PLUGIN_OSSIA_EXPORT ProcessComponentFactory :
        public iscore::GenericComponentFactory<
            Process::ProcessModel,
            LocalTree::DocumentPlugin,
            LocalTree::ProcessComponentFactory>
{
        ISCORE_ABSTRACT_FACTORY("0732ab51-a052-4e2e-a1f7-9bf2926c199c")
    public:
        virtual ~ProcessComponentFactory();
        virtual ProcessComponent* make(
                const Id<iscore::Component>&,
                OSSIA::Node& parent,
                Process::ProcessModel& proc,
                LocalTree::DocumentPlugin& doc,
                QObject* paren_objt) const = 0;
};

template<typename ProcessComponent_T>
class ProcessComponentFactory_T :
        public ProcessComponentFactory
{
    public:
        using model_type = typename ProcessComponent_T::model_type;
        using component_type = ProcessComponent_T;
        virtual ~ProcessComponentFactory_T() = default;

        static auto static_concreteFactoryKey()
        {
            return ProcessComponent_T::static_key().impl();
        }

        ConcreteFactoryKey concreteFactoryKey() const final override
        {
            return ProcessComponent_T::static_key().impl(); // Note : here there is a conversion between UuidKey<Component> and ConcreteFactoryKey
        }

        bool matches(
                Process::ProcessModel& p,
                const DocumentPlugin&) const override
        {
            return dynamic_cast<model_type*>(&p);
        }

        ProcessComponent* make(
                const Id<iscore::Component>& id,
                OSSIA::Node& parent,
                Process::ProcessModel& proc,
                DocumentPlugin& doc,
                QObject* paren_objt) const override
        {
            return new ProcessComponent_T{id, parent, static_cast<model_type&>(proc), doc, paren_objt};
        }
};

using ProcessComponentFactoryList =
    iscore::GenericComponentFactoryList<
            Process::ProcessModel,
            LocalTree::DocumentPlugin,
            LocalTree::ProcessComponentFactory>;
}
}
