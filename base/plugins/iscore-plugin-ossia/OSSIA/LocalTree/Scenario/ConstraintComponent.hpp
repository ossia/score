#pragma once
#include <OSSIA/LocalTree/Scenario/ProcessComponent.hpp>
#include <Scenario/Document/Components/ConstraintComponent.hpp>

namespace OSSIA
{
namespace LocalTree
{
class ConstraintComponent final :
        public iscore::Component
{
    public:
        using system_t = OSSIA::LocalTree::DocumentPlugin;
        using process_component_t = OSSIA::LocalTree::ProcessComponent;
        using process_component_factory_t = OSSIA::LocalTree::ProcessComponentFactory;
        using process_component_factory_list_t = OSSIA::LocalTree::ProcessComponentFactoryList;

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
                Process &process,
                const DocumentPlugin &system,
                const iscore::DocumentContext &ctx,
                QObject *parent_component);

        void removing(const Process& cst, const ProcessComponent& comp);

        auto& node() const
        { return m_thisNode; }

    private:
        std::shared_ptr<OSSIA::Node> m_thisNode;
        std::shared_ptr<OSSIA::Node> m_processesNode;
        std::vector<BaseProperty*> m_properties;
        parent_t m_baseComponent;
};



}
}
