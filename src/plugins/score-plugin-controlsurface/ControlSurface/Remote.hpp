#pragma once
#include <ControlSurface/Process.hpp>

#include <QMetaObject>

#include <RemoteControl/Scenario/Process.hpp>

namespace ControlSurface
{
class Remote : public RemoteControl::ProcessComponent_T<Model>
{
  COMPONENT_METADATA("9bd540a2-79e1-4bb8-b9f6-7e775b4616dd")

public:
  Remote(
      Model& scenario,
      RemoteControl::DocumentPlugin& doc,
      const Id<score::Component>& id,
      QObject* parent_obj);
};


using RemoteFactory = RemoteControl::ProcessComponentFactory_T<Remote>;
}
