#pragma once
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <OSSIA/LocalTree/NameProperty.hpp>
#include <iscore/component/Component.hpp>
#include <iscore/component/ComponentFactory.hpp>

// TODO clean me up
namespace Ossia
{
namespace LocalTree
{
class ISCORE_PLUGIN_OSSIA_EXPORT ProcessComponent : public iscore::Component
{
    public:
        ProcessComponent(
                OSSIA::Node& node,
                Process::ProcessModel& proc,
                const Id<iscore::Component>& id,
                const QString& name,
                QObject* parent);

        virtual ~ProcessComponent();
        auto& node() const
        { return m_thisNode.node; }

    private:
        OSSIA::Node& thisNode() const
        { return *node(); }
        MetadataNamePropertyWrapper m_thisNode;
};

class ISCORE_PLUGIN_OSSIA_EXPORT ProcessComponentFactory :
        public iscore::GenericComponentFactory<
            Process::ProcessModel,
            LocalTree::DocumentPlugin,
            LocalTree::ProcessComponentFactory>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                LocalTree::ProcessComponentFactory,
                "0732ab51-a052-4e2e-a1f7-9bf2926c199c")
    public:
        virtual ~ProcessComponentFactory();
        virtual ProcessComponent* make(
                const Id<iscore::Component>&,
                OSSIA::Node& parent,
                Process::ProcessModel& proc,
                LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt) const = 0;
};

using ProcessComponentFactoryList =
    iscore::GenericComponentFactoryList<
            Process::ProcessModel,
            LocalTree::DocumentPlugin,
            LocalTree::ProcessComponentFactory>;
}
}
