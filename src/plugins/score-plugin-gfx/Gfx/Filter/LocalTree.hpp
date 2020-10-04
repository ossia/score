#pragma once
#include <LocalTree/LocalTreeComponent.hpp>
#include <LocalTree/ProcessComponent.hpp>

namespace Gfx::Filter
{
class Model;

class LocalTreeProcessComponent : public LocalTree::ProcessComponent_T<Model>
{
  COMPONENT_METADATA("18884f6a-86ed-4cd0-9c1c-fd29018283ea")

public:
  LocalTreeProcessComponent(
      const Id<score::Component>& id,
      ossia::net::node_base& parent,
      Gfx::Filter::Model& scenario,
      const score::DocumentContext& doc,
      QObject* parent_obj);

  ~LocalTreeProcessComponent() override;
};

using LocalTreeProcessComponentFactory
    = LocalTree::ProcessComponentFactory_T<LocalTreeProcessComponent>;
}
