#pragma once
#include <Scenario/Document/Components/ProcessComponent.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <OSSIA/LocalTree/NameProperty.hpp>

// TODO clean me up
namespace OSSIA
{
namespace LocalTree
{
class ISCORE_PLUGIN_OSSIA_EXPORT ProcessComponent : public iscore::Component
{
        ISCORE_METADATA(OSSIA::LocalTree::ProcessComponent)
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
        public ::GenericComponentFactory<
            Process::ProcessModel,
            LocalTree::DocumentPlugin,
            LocalTree::ProcessComponent>
{
    public:
        virtual ~ProcessComponentFactory();
        virtual ProcessComponent* make(
                const Id<iscore::Component>&,
                OSSIA::Node& parent,
                Process::ProcessModel& proc,
                const LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt) const = 0;
};

using ProcessComponentFactoryList =
    ::GenericComponentFactoryList<
            Process::ProcessModel,
            LocalTree::DocumentPlugin,
            LocalTree::ProcessComponent,
            LocalTree::ProcessComponentFactory>;
}
}
