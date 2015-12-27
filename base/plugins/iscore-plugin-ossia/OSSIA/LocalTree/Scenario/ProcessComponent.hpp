#pragma once
#include <Scenario/Document/Components/ProcessComponent.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <OSSIA/LocalTree/NameProperty.hpp>
namespace OSSIA
{
namespace LocalTree
{
class ProcessComponent : public iscore::Component
{
        ISCORE_METADATA(OSSIA::LocalTree::ProcessComponent)
    public:
        ProcessComponent(
                OSSIA::Node& node,
                Process& proc,
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

class ProcessComponentFactory :
        public ::GenericProcessComponentFactory<
            LocalTree::DocumentPlugin,
            LocalTree::ProcessComponent>
{
    public:
        virtual ProcessComponent* make(
                const Id<iscore::Component>&,
                OSSIA::Node& parent,
                Process& proc,
                const LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt) const = 0;
};

using ProcessComponentFactoryList =
    ::GenericProcessComponentFactoryList<
            LocalTree::DocumentPlugin,
            LocalTree::ProcessComponent,
            LocalTree::ProcessComponentFactory>;
}
}
