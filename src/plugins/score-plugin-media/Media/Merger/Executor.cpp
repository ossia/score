#include "Executor.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <ossia/dataflow/nodes/merger.hpp>
#include <ossia/editor/scenario/time_value.hpp>
namespace Execution
{

MergerComponent::MergerComponent(
    Media::Merger::Model& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : Execution::ProcessComponent_T<Media::Merger::Model, ossia::node_process>{
        element,
        ctx,
        id,
        "Executor::MergerComponent",
        parent}
{
  auto node = std::make_shared<ossia::nodes::merger>(element.inCount());
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  // TODO change num of ins dynamically
}

void MergerComponent::recompute() { }

MergerComponent::~MergerComponent() { }
}
