#pragma once
#include <OSSIA/LocalTree/Scenario/ProcessComponent.hpp>
#include <OSSIA/LocalTree/LocalTreeComponent.hpp>
#include <Scenario/Document/Components/ConstraintComponent.hpp>

namespace Ossia
{
namespace LocalTree
{
class ConstraintBase :
        public Component<Scenario::GenericConstraintComponent<DocumentPlugin>>
{
        COMPONENT_METADATA("11d928b5-eaeb-471c-b3b7-dc453180b10f")
    public:
        using parent_t = Component<Scenario::GenericConstraintComponent<DocumentPlugin>>;
        using system_t = Ossia::LocalTree::DocumentPlugin;
        using process_component_t = Ossia::LocalTree::ProcessComponent;
        using process_component_factory_t = Ossia::LocalTree::ProcessComponentFactory;
        using process_component_factory_list_t = Ossia::LocalTree::ProcessComponentFactoryList;

        ConstraintBase(
                OSSIA::Node& parent,
                const Id<iscore::Component>& id,
                Scenario::ConstraintModel& constraint,
                system_t& doc,
                QObject* parent_comp);

        ProcessComponent* make_processComponent(
                const Id<iscore::Component> & id,
                ProcessComponentFactory& factory,
                Process::ProcessModel &process);

        void removing(const Process::ProcessModel& cst, const ProcessComponent& comp);

    private:
        std::shared_ptr<OSSIA::Node> m_processesNode;
};



class Constraint final : public ConstraintComponentHierarchyManager<
    ConstraintBase,
    ConstraintBase::process_component_t,
    ConstraintBase::process_component_factory_list_t
>
{
    public:
        using hierarchy_t::ConstraintComponentHierarchyManager;

};
}
}
