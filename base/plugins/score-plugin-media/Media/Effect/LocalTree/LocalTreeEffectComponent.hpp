#pragma once
/*
#include <Engine/LocalTree/LocalTreeComponent.hpp>
#include <Process/Process.hpp>
#include <Effect/EffectComponent.hpp>
#include <score/model/ComponentFactory.hpp>
#include <score/plugins/customfactory/ModelFactory.hpp>

namespace Media
{
namespace Effect
{
namespace LocalTree
{
class EffectComponent :
        public Engine::LocalTree::Component<GenericEffectComponent<Engine::LocalTree::DocumentPlugin>>
{
        Q_OBJECT
        COMMON_COMPONENT_METADATA("c9e1f9bc-b974-4695-a3a8-f797c34858ee")
    public:
        using parent_t = Engine::LocalTree::Component<GenericEffectComponent<Engine::LocalTree::DocumentPlugin>>;
        static const constexpr bool is_unique = true;
        EffectComponent(
                ossia::net::node_base& node,
                Process::EffectModel& proc,
                Engine::LocalTree::DocumentPlugin& doc,
                const Id<score::Component>& id,
                const QString& name,
                QObject* parent);

        virtual ~EffectComponent();

        void recreate();

        ossia::net::node_base& parameters() const
        { return m_parametersNode; }

        const auto& inAddresses() const { return m_inAddresses; }
        const auto& outAddresses() const { return m_outAddresses; }
    signals:
        void effectTreeChanged();
        void aboutToBeDestroyed();

    protected:
        void on_nodeDeleted(const ossia::net::node_base&);

        ossia::net::node_base& m_parametersNode;
        std::vector<std::tuple<int32_t, ossia::net::parameter_base*, ossia::net::node_base*>> m_inAddresses;
        std::vector<std::tuple<int32_t, ossia::net::parameter_base*, ossia::net::node_base*>> m_outAddresses;
};

}
}
}

*/
