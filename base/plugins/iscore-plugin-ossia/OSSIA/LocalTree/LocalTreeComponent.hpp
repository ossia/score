#pragma once
#include <OSSIA/LocalTree/NameProperty.hpp>
#include <OSSIA/LocalTree/Scenario/MetadataParameters.hpp>
#include <iscore/component/Component.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
namespace Ossia
{
namespace LocalTree
{
template<typename Component_T>
class Component :
        public Component_T
{
    public:
        template<typename... Args>
        Component(OSSIA::net::Node& n, ModelMetadata& m, Args&&... args):
            Component_T{std::forward<Args>(args)...},
            m_thisNode{n, m, this}
        {
            make_metadata_node(m, m_thisNode.node, m_properties, this);
        }

        auto& node() const
        { return m_thisNode.node; }

    protected:
        MetadataNamePropertyWrapper m_thisNode;
        std::vector<std::unique_ptr<BaseProperty>> m_properties;
};

using CommonComponent = Component<iscore::GenericComponent<DocumentPlugin>>;
}
}
