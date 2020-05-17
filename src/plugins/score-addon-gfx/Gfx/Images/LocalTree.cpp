#include "LocalTree.hpp"

#include <Gfx/Images/Process.hpp>
#include <LocalTree/Property.hpp>

namespace Gfx::Images
{
LocalTreeProcessComponent::LocalTreeProcessComponent(
    const Id<score::Component>& id,
    ossia::net::node_base& parent,
    Gfx::Images::Model& proc,
    const score::DocumentContext& sys,
    QObject* parent_obj)
    : LocalTree::ProcessComponent_T<Gfx::Images::Model>{parent,
                                                       proc,
                                                       sys,
                                                       id,
                                                       "gfxComponent",
                                                       parent_obj}
{
}

LocalTreeProcessComponent::~LocalTreeProcessComponent() {}
}
