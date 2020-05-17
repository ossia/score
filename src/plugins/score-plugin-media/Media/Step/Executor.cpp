#include "Executor.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/ExecutionContext.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/dataflow/nodes/step.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/editor/scenario/time_value.hpp>
namespace Execution
{

StepComponent::StepComponent(
    Media::Step::Model& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : Execution::ProcessComponent_T<Media::Step::Model, ossia::node_process>{
        element,
        ctx,
        id,
        "Executor::StepComponent",
        parent}
{
  auto node = std::make_shared<ossia::nodes::step>();
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);
  node->dur = ossia::time_value{int64_t(element.stepDuration())};

  recompute();
  con(element, &Media::Step::Model::stepsChanged, this, &StepComponent::recompute);
  con(element, &Media::Step::Model::minChanged, this, &StepComponent::recompute);
  con(element, &Media::Step::Model::maxChanged, this, &StepComponent::recompute);
  con(element, &Media::Step::Model::stepDurationChanged, this, [=] {
    in_exec([node, dur = process().stepDuration()]() mutable {
      node->dur = ossia::time_value{int64_t(dur)};
    });
  });
}

void StepComponent::recompute()
{
  float min = process().min();
  float max = process().max();
  ossia::float_vector v = process().steps();
  for (auto& val : v)
  {
    val = min + (1. - val) * (max - min);
  }
  in_exec([n = std::dynamic_pointer_cast<ossia::nodes::step>(OSSIAProcess().node),
           vec = std::move(v)]() mutable { n->values = std::move(vec); });
}

StepComponent::~StepComponent() { }
}
