#pragma once
#include <LocalTree/LocalTreeComponent.hpp>
#include <LocalTree/Scenario/ProcessComponent.hpp>

namespace Skeleton
{
class Model;

class LocalTreeProcessComponent
    : public Engine::LocalTree::ProcessComponent_T<Model>
{
  COMPONENT_METADATA("00000000-0000-0000-0000-000000000000")

public:
  LocalTreeProcessComponent(
      const Id<score::Component>& id,
      ossia::net::node_base& parent,
      Skeleton::Model& scenario,
      Engine::LocalTree::DocumentPlugin& doc,
      QObject* parent_obj);

  ~LocalTreeProcessComponent();
};

using LocalTreeProcessComponentFactory
    = Engine::LocalTree::ProcessComponentFactory_T<LocalTreeProcessComponent>;
}
