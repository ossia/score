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
  auto np = std::make_shared<ossia::node_process>(node);
  m_node = node;
  node->values = element.steps();
  node->dur = ctx.time(element.stepDuration());

  if(auto dest = Engine::score_to_ossia::makeDestination(ctx.devices.list(), element.outlet->address()))
    node->outputs()[0]->address = &dest->address();

  ctx.plugin.register_node(element, node);

  ctx.plugin.outlets.insert({process().outlet.get(), std::make_pair(node, node->outputs()[0])});
  ctx.plugin.execGraph->add_node(m_node);
  m_ossia_process = np;
}

void StepComponent::recompute()
{
  system().executionQueue.enqueue(
        [n=std::dynamic_pointer_cast<step_node>(this->m_node)
        ]
  {
  });
}

StepComponent::~StepComponent()
{
  system().plugin.unregister_node(process(), m_node);
}

}
}

