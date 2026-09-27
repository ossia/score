#include "Executor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/dataflow/node_process.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <Media/Step/StepNode.hpp>

#include <QTimer>
namespace Execution
{

//! The steps as sent: 0 is max and 1 min, as drawn.
static std::vector<ossia::float_vector> mappedSequences(const Media::Step::Model& m)
{
  const float min = m.min();
  const float max = m.max();
  auto seqs = m.sequences();
  for(auto& seq : seqs)
    for(auto& val : seq)
      val = min + (1. - val) * (max - min);
  return seqs;
}

StepComponent::StepComponent(
    Media::Step::Model& element, const Execution::Context& ctx, QObject* parent)
    : Execution::ProcessComponent_T<Media::Step::Model, ossia::node_process>{
        element, ctx, "Executor::StepComponent", parent}
{
  using Media::Step::step_node;
  auto node = ossia::make_node<step_node>(*ctx.execState);
  node->sequences = mappedSequences(element);
  node->current_sequence = element.currentSequence();
  node->switch_rate = ossia::convert<float>(element.switchQuantification->value());
  node->set_duration(element.stepDuration->value());
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  element.sequenceSelect->setupExecution(node->sequence_select, this);
  element.switchQuantification->setupExecution(node->switch_quantification, this);
  element.stepDuration->setupExecution(node->duration, this);

  // The inspector and the layer write the ports' values; cables and
  // automations reach the node's inlets directly.
  con(*element.sequenceSelect, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, c = ossia::convert<int>(v)] { node->request_sequence(c); });
  });
  con(*element.switchQuantification, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, r = double(ossia::convert<float>(v))] { node->switch_rate = r; });
  });
  con(*element.stepDuration, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, v] { node->set_duration(v); });
  });

  con(element, &Media::Step::Model::stepsChanged, this, &StepComponent::recompute);
  con(element, &Media::Step::Model::minChanged, this, &StepComponent::recompute);
  con(element, &Media::Step::Model::maxChanged, this, &StepComponent::recompute);

  con(ctx.doc.execTimer, &QTimer::timeout, this,
      [&element, node] { element.execPosition(node->last.load(std::memory_order_relaxed)); });
}

void StepComponent::recompute()
{
  in_exec([n = std::dynamic_pointer_cast<Media::Step::step_node>(OSSIAProcess().node),
           seqs = mappedSequences(process())]() mutable {
    // Swap, not move: the old list is freed with the command, on the UI thread.
    std::swap(n->sequences, seqs);
  });
}

void StepComponent::stop()
{
  ProcessComponent::stop();
  process().execPosition(-1);
}

StepComponent::~StepComponent() { }
}
