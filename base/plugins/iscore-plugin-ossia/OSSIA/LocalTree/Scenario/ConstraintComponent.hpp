#pragma once
#include <OSSIA/LocalTree/Scenario/ProcessComponent.hpp>
#include <OSSIA/LocalTree/LocalTreeComponent.hpp>
#include <Scenario/Document/Components/ConstraintComponent.hpp>
#include <iscore/component/ComponentHierarchy.hpp>
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
        using model_t = Process::ProcessModel;
        using component_t = Ossia::LocalTree::ProcessComponent;
        using component_factory_t = Ossia::LocalTree::ProcessComponentFactory;
        using component_factory_list_t = Ossia::LocalTree::ProcessComponentFactoryList;

        ConstraintBase(
                ossia::net::Node& parent,
                const Id<iscore::Component>& id,
                Scenario::ConstraintModel& constraint,
                DocumentPlugin& sys,
                QObject* parent_comp);

        ProcessComponent* make(
                const Id<iscore::Component> & id,
                ProcessComponentFactory& factory,
                Process::ProcessModel &process);

        void removing(const Process::ProcessModel& cst, const ProcessComponent& comp);

    private:
        ossia::net::Node& m_processesNode;
};



class Constraint final :
        public iscore::PolymorphicComponentHierarchy<ConstraintBase>
{
    public:
        using iscore::PolymorphicComponentHierarchy<ConstraintBase>::PolymorphicComponentHierarchyManager;

};
}
}
