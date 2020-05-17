#pragma once
#include <Media/Merger/Model.hpp>
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>
namespace Execution
{

class MergerComponent final
    : public ::Execution::ProcessComponent_T<Media::Merger::Model, ossia::node_process>
{
  COMPONENT_METADATA("5e0fbbfd-3d7f-40b3-92eb-dfeddc8d3c84")
public:
  MergerComponent(
      Media::Merger::Model& element,
      const ::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void recompute();

  ~MergerComponent();

private:
};

using MergerComponentFactory = ::Execution::ProcessComponentFactory_T<MergerComponent>;
}
