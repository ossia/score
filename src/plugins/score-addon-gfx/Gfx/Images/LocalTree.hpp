#pragma once
#include <LocalTree/LocalTreeComponent.hpp>
#include <LocalTree/ProcessComponent.hpp>

namespace Gfx::Images
{
class Model;

class LocalTreeProcessComponent : public LocalTree::ProcessComponent_T<Model>
{
  COMPONENT_METADATA("a6010842-9b36-43d2-978e-50273935d21b")

public:
  LocalTreeProcessComponent(
      const Id<score::Component>& id,
      ossia::net::node_base& parent,
      Gfx::Images::Model& scenario,
      const score::DocumentContext& doc,
      QObject* parent_obj);

  ~LocalTreeProcessComponent() override;
};

using LocalTreeProcessComponentFactory
    = LocalTree::ProcessComponentFactory_T<LocalTreeProcessComponent>;
}
