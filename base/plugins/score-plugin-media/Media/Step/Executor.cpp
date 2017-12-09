#include "Executor.hpp"
#include <Engine/score2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia/editor/scenario/time_value.hpp>
namespace Engine
{
namespace Execution
{

class OSSIA_EXPORT step_node final :
    public ossia::graph_node
{
public:
  step_node();
  ~step_node();

  void run(ossia::token_request t, ossia::execution_state& e) override;
  std::vector<float> values;
  ossia::time_value dur{};
};

step_node::step_node()
{
  m_outlets.push_back(ossia::make_outlet<ossia::value_port>());
}

step_node::~step_node()
{

}

void step_node::run(ossia::token_request t, ossia::execution_state& e)
{
  // We want to send a trigger for each value change that happened between last_t and now
  if(t.date > m_prev_date)
  {
    auto& port = *m_outlets[0]->data.target<ossia::value_port>();

    // TODO optimizeme... quite naive for now.
    // TODO maybe start from m_prev_date + 1 ?
    for(int64_t i = m_prev_date.impl; i < t.date.impl; i++)
    {
      if(i % dur == 0)
      {
        port.add_value(values[(i / dur) % values.size()], i - m_prev_date + t.offset);
      }
    }
  }
}

StepComponent::StepComponent(
    Media::Step::Model &element,
    const Engine::Execution::Context &ctx,
    const Id<score::Component> &id,
    QObject *parent)
  : Engine::Execution::ProcessComponent_T<Media::Step::Model, ossia::node_process>{
      element,
      ctx,
      id, "Executor::StepComponent", parent}
{
  auto node = std::make_shared<step_node>();
  m_ossia_process = std::make_shared<ossia::node_process>(node);
  node->dur = ossia::time_value(element.stepDuration());

  recompute();
  con(element, &Media::Step::Model::stepsChanged,
      this, &StepComponent::recompute);
  con(element, &Media::Step::Model::minChanged,
      this, &StepComponent::recompute);
  con(element, &Media::Step::Model::maxChanged,
      this, &StepComponent::recompute);
  con(element, &Media::Step::Model::stepDurationChanged,
      this, [=] {
    system().executionQueue.enqueue(
          [node,dur=process().stepDuration()] () mutable
    {
      node->dur = ossia::time_value(dur);
    });
  });
  ctx.plugin.register_node(element, node);
}

void StepComponent::recompute()
{
  float min = process().min();
  float max = process().max();
  std::vector<float> v = process().steps();
  for(auto& val : v)
  {
    val = min + (1. - val) * (max - min);
  }
  system().executionQueue.enqueue(
        [n=std::dynamic_pointer_cast<step_node>(OSSIAProcess().node),vec=std::move(v)] () mutable
  {
    n->values = std::move(vec);
  });
}

StepComponent::~StepComponent()
{
  system().plugin.unregister_node(process(), OSSIAProcess().node);
}

}
}

