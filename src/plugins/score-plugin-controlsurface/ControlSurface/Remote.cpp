#include <ControlSurface/Remote.hpp>

namespace ControlSurface
{

Remote::Remote(
    Model& scenario,
    RemoteControl::DocumentPlugin& doc,
    const Id<score::Component>& id,
    QObject* parent_obj)
    : RemoteControl::ProcessComponent_T<Model>{
        scenario, doc, id, "ControlSurfaceComponent", parent_obj}
{
}

}
