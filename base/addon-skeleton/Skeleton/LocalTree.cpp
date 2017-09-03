#include "LocalTree.hpp"
#include <Skeleton/Process.hpp>
#include <Engine/LocalTree/Property.hpp>

namespace Skeleton
{
LocalTreeProcessComponent::LocalTreeProcessComponent(
        const Id<iscore::Component>& id,
        ossia::net::node_base& parent,
        Skeleton::Model& proc,
        Engine::LocalTree::DocumentPlugin& sys,
        QObject* parent_obj):
    Engine::LocalTree::ProcessComponent_T<Skeleton::Model>{parent, proc, sys, id, "SkeletonComponent", parent_obj}
{
}

LocalTreeProcessComponent::~LocalTreeProcessComponent()
{

}

}
