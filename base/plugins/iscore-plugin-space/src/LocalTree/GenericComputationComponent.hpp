#pragma once
#include <src/LocalTree/ComputationComponent.hpp>
#include <OSSIA/LocalTree/LocalTreeDocumentPlugin.hpp>
namespace Space
{
namespace LocalTree
{


class GenericComputationComponent final : public ComputationComponent
{
        COMPONENT_METADATA(GenericComputationComponent)
    public:
        GenericComputationComponent(
                const Id<iscore::Component>& cmp,
                OSSIA::Node& parent_node,
                ComputationModel& proc,
                const Ossia::LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt);
};

}
}
