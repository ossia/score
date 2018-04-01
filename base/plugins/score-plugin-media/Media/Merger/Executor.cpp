#include "Executor.hpp"
#include <Engine/score2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/dataflow/nodes/merger.hpp>
namespace Engine
{
namespace Execution
{


MergerComponent::MergerComponent(
    Media::Merger::Model &element,
    const Engine::Execution::Context &ctx,
    const Id<score::Component> &id,
    QObject *parent)
  : Engine::Execution::ProcessComponent_T<Media::Merger::Model, ossia::node_process>{
      element,
      ctx,
      id, "Executor::MergerComponent", parent}
{
  auto node = std::make_shared<ossia::nodes::merger>(element.inCount());
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  // TODO change num of ins dynamically
}

void MergerComponent::recompute()
{
}

MergerComponent::~MergerComponent()
{
}

}
}

