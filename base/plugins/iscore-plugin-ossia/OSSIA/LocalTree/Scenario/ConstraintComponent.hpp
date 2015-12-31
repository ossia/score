#pragma once
#include <OSSIA/LocalTree/Scenario/ProcessComponent.hpp>
#include <Scenario/Document/Components/ConstraintComponent.hpp>

namespace Ossia
{
namespace LocalTree
{
class ConstraintComponent final :
        public iscore::Component
{
    public:
        using system_t = Ossia::LocalTree::DocumentPlugin;
        using process_component_t = Ossia::LocalTree::ProcessComponent;
        using process_component_factory_t = Ossia::LocalTree::ProcessComponentFactory;
        using process_component_factory_list_t = Ossia::LocalTree::ProcessComponentFactoryList;

        using parent_t = ::ConstraintComponentHierarchyManager<
            ConstraintComponent,
            system_t,
            process_component_t,
            process_component_factory_list_t
        >;

        const Key& key() const override;

        ConstraintComponent(
                OSSIA::Node& parent,
                const Id<Component>& id,
                ConstraintModel& constraint,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp);
        ~ConstraintComponent();

        ProcessComponent* make_processComponent(
                const Id<Component> & id,
                ProcessComponentFactory& factory,
                Process::ProcessModel &process,
                const DocumentPlugin &system,
                const iscore::DocumentContext &ctx,
                QObject *parent_component);

        void removing(const Process::ProcessModel& cst, const ProcessComponent& comp);

        auto& node() const
        { return m_thisNode.node; }

    private:
        OSSIA::Node& thisNode() const
        { return *node(); }

        MetadataNamePropertyWrapper m_thisNode;
        std::shared_ptr<OSSIA::Node> m_processesNode;
        std::vector<std::unique_ptr<BaseProperty>> m_properties;
        parent_t m_baseComponent;
};



}
}
