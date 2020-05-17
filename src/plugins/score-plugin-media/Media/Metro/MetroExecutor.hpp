#pragma once
#include <Media/Metro/MetroModel.hpp>
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Execution
{

class MetroComponent final
    : public ::Execution::ProcessComponent_T<Media::Metro::Model, ossia::node_process>
{
  COMPONENT_METADATA("f36a8077-b60a-42f2-8250-b581d335fd17")
public:
  MetroComponent(
      Media::Metro::Model& element,
      const ::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void recompute();

  ~MetroComponent();

private:
};

using MetroComponentFactory = ::Execution::ProcessComponentFactory_T<MetroComponent>;
}
