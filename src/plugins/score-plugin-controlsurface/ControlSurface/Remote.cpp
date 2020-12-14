#include <ControlSurface/Remote.hpp>

namespace ControlSurface
{

Remote::Remote(
    Model& proc,
    RemoteControl::DocumentPlugin& doc,
    const Id<score::Component>& id,
    QObject* parent_obj)
    : RemoteControl::ProcessComponent_T<Model>{
        proc, doc, id, "ControlSurfaceComponent", parent_obj}
{
  //con(proc, &Model::startExecution);
}

}
