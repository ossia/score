#pragma once
#include <Scenario/Document/State/StateModel.hpp>
#include <Network/Node.h>
#include <OSSIA/LocalTree/BaseProperty.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>


namespace OSSIA
{
namespace LocalTree
{
class StateComponent final :
        public iscore::Component
{
        std::shared_ptr<OSSIA::Node> m_thisNode;
        std::vector<BaseProperty*> m_properties;

    public:
        using system_t = OSSIA::LocalTree::DocumentPlugin;

        StateComponent(
                OSSIA::Node& parent,
                const Id<iscore::Component>& id,
                StateModel& state,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp);

        const Key& key() const override;

        auto& node() const
        { return m_thisNode; }
        ~StateComponent();
};
}
}
