#pragma once
#include <Scenario/Document/State/StateModel.hpp>
#include <Network/Node.h>
#include <OSSIA/LocalTree/BaseProperty.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <OSSIA/LocalTree/NameProperty.hpp>

namespace Ossia
{
namespace LocalTree
{
class StateComponent final :
        public iscore::Component
{
    public:
        using system_t = Ossia::LocalTree::DocumentPlugin;

        StateComponent(
                OSSIA::Node& parent,
                const Id<iscore::Component>& id,
                Scenario::StateModel& state,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp);

        const Key& key() const override;

        auto& node() const
        { return m_thisNode.node; }
        ~StateComponent();

    private:
        OSSIA::Node& thisNode() const
        { return *node(); }

        MetadataNamePropertyWrapper m_thisNode;
        std::vector<std::unique_ptr<BaseProperty>> m_properties;

};
}
}
