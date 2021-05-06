#include "LocalTree.hpp"

#include <ControlSurface/Process.hpp>
#include <LocalTree/Property.hpp>

namespace ControlSurface
{
LocalTreeProcessComponent::LocalTreeProcessComponent(
    ossia::net::node_base& parent,
    ControlSurface::Model& proc,
    const score::DocumentContext& sys,
    QObject* parent_obj)
    : LocalTree::ProcessComponent_T<ControlSurface::Model>{
        parent,
        proc,
        sys,
        "ControlSurfaceComponent",
        parent_obj}
{
}

LocalTreeProcessComponent::~LocalTreeProcessComponent() { }
}
