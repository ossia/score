#include "LocalTree.hpp"

#include <LocalTree/Property.hpp>
#include <ControlSurface/Process.hpp>

namespace ControlSurface
{
LocalTreeProcessComponent::LocalTreeProcessComponent(
    const Id<score::Component>& id, ossia::net::node_base& parent,
    ControlSurface::Model& proc, LocalTree::DocumentPlugin& sys, QObject* parent_obj)
    : LocalTree::ProcessComponent_T<ControlSurface::Model>{
          parent, proc, sys, id, "ControlSurfaceComponent", parent_obj}
{
}

LocalTreeProcessComponent::~LocalTreeProcessComponent()
{
}
}
