#pragma once
#include <src/LocalTree/AreaComponent.hpp>

namespace Space
{
namespace LocalTree
{

class GenericAreaComponent final : public AreaComponent
{
        COMPONENT_METADATA(GenericAreaComponent)
    public:
        GenericAreaComponent(
                const Id<iscore::Component>& cmp,
                OSSIA::Node& parent_node,
                AreaModel& proc,
                const Ossia::LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt);
};
}
}
