#pragma once
#include <Scenario/Document/Components/ScenarioComponent.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
namespace OSSIA
{
namespace LocalTree
{

class ProcessComponent : public Component
{
        ISCORE_METADATA(OSSIA::LocalTree::ProcessComponent)
    public:
        using Component::Component;
};

class ProcessComponentFactory :
        public ::GenericProcessComponentFactory<
            LocalTree::DocumentPlugin,
            LocalTree::ProcessComponent>
{
    public:
        virtual ProcessComponent* make(
                const Id<Component>&,
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
