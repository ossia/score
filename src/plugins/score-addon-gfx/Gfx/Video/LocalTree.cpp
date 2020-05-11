#include "LocalTree.hpp"

#include <Gfx/Video/Process.hpp>
#include <LocalTree/Property.hpp>

namespace Gfx::Video
{
LocalTreeProcessComponent::LocalTreeProcessComponent(
    const Id<score::Component>& id,
    ossia::net::node_base& parent,
    Gfx::Video::Model& proc,
    const score::DocumentContext& sys,
    QObject* parent_obj)
    : LocalTree::ProcessComponent_T<Gfx::Video::Model>{parent,
                                                       proc,
                                                       sys,
                                                       id,
                                                       "gfxComponent",
                                                       parent_obj}
{
}

LocalTreeProcessComponent::~LocalTreeProcessComponent() {}
}
