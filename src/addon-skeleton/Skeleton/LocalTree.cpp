#include "LocalTree.hpp"

#include <LocalTree/Property.hpp>
#include <Skeleton/Process.hpp>

namespace Skeleton
{
LocalTreeProcessComponent::LocalTreeProcessComponent(
    ossia::net::node_base& parent,
    Skeleton::Model& proc, const score::DocumentContext& sys, QObject* parent_obj)
    : LocalTree::ProcessComponent_T<Skeleton::Model>{
          parent, proc, sys, "SkeletonComponent", parent_obj}
{
}

LocalTreeProcessComponent::~LocalTreeProcessComponent()
{
}
}
