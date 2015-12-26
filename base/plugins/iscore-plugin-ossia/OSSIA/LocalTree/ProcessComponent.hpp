#pragma once
#include <Scenario/Document/Components/ScenarioComponent.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
namespace OSSIA
{
namespace LocalTree
{

class ProcessComponent : public iscore::Component
{
        ISCORE_METADATA(OSSIA::LocalTree::ProcessComponent)
    public:
        ProcessComponent(
                const std::shared_ptr<OSSIA::Node>& node,
                const Id<iscore::Component>& id,
                const QString& name,
                QObject* parent):
        Component{id, name, parent},
            m_thisNode{node}
        {
        }

        auto& node() const
        {
            return m_thisNode;
        }

    private:
        std::shared_ptr<OSSIA::Node> m_thisNode;
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
