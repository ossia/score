#pragma once
#include <LocalTree/LocalTreeComponent.hpp>
#include <LocalTree/ProcessComponent.hpp>

namespace Gfx::Video
{
class Model;

class LocalTreeProcessComponent : public LocalTree::ProcessComponent_T<Model>
{
  COMPONENT_METADATA("f59d2fba-b65f-4123-b5b7-1740f459aab2")

public:
  LocalTreeProcessComponent(
      const Id<score::Component>& id,
      ossia::net::node_base& parent,
      Gfx::Video::Model& scenario,
      const score::DocumentContext& doc,
      QObject* parent_obj);

  ~LocalTreeProcessComponent() override;
};

using LocalTreeProcessComponentFactory
    = LocalTree::ProcessComponentFactory_T<LocalTreeProcessComponent>;
}
