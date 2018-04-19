#include "LocalTree.hpp"

#include <Engine/LocalTree/Property.hpp>
#include <Skeleton/Process.hpp>

namespace Skeleton
{
LocalTreeProcessComponent::LocalTreeProcessComponent(
    const Id<score::Component>& id,
    ossia::net::node_base& parent,
    Skeleton::Model& proc,
    Engine::LocalTree::DocumentPlugin& sys,
    QObject* parent_obj)
    : Engine::LocalTree::ProcessComponent_T<Skeleton::Model>{
          parent, proc, sys, id, "SkeletonComponent", parent_obj}
{
}

LocalTreeProcessComponent::~LocalTreeProcessComponent()
{
}
}
