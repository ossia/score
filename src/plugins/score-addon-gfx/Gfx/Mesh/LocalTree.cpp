#include "LocalTree.hpp"

#include <Gfx/Mesh/Process.hpp>
#include <LocalTree/Property.hpp>

namespace Gfx::Mesh
{
LocalTreeProcessComponent::LocalTreeProcessComponent(
    const Id<score::Component>& id,
    ossia::net::node_base& parent,
    Gfx::Mesh::Model& proc,
    const score::DocumentContext& sys,
    QObject* parent_obj)
    : LocalTree::ProcessComponent_T<Gfx::Mesh::Model>{
        parent,
        proc,
        sys,
        id,
        "gfxComponent",
        parent_obj}
{
}

LocalTreeProcessComponent::~LocalTreeProcessComponent() { }
}
