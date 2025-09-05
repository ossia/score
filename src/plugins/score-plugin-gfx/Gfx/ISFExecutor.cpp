#include "ISFExecutor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/ISFExecutorNode.hpp>
#include <Gfx/ShaderProgram.hpp>
#include <Gfx/TexturePort.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/dataflow/graph_edge_helpers.hpp>
#include <ossia/dataflow/port.hpp>
namespace Gfx
{
void ISFExecutorComponent::init(
    const Gfx::ProcessedProgram& shader, const Execution::Context& ctx)
try
{
  const auto& desc = shader.descriptor;

  auto n = ossia::make_node<filter_node>(
      *ctx.execState, desc, shader.vertex, shader.fragment,
      ctx.doc.plugin<DocumentPlugin>().exec);

  for(auto* outlet : process().outlets())
  {
    if(auto out = qobject_cast<TextureOutlet*>(outlet))
    {
      out->nodeId = n->id;
    }
  }

  this->node = n;

  Execution::Transaction commands{system()};
  setup_node(commands);
  commands.run_all_in_exec();

  m_ossia_process = std::make_shared<ossia::node_process>(this->node);

  m_oldInlets = process().inlets();
  m_oldOutlets = process().outlets();
}
catch(...)
{
}

void ISFExecutorComponent::cleanup()
{
  for(auto* outlet : this->process().outlets())
  {
    if(auto out = qobject_cast<TextureOutlet*>(outlet))
    {
      out->nodeId = -1;
    }
  }
}

void ISFExecutorComponent::on_shaderChanged(const Gfx::ProcessedProgram& shader)
{
  auto& setup = system().setup;
  Execution::Transaction commands{system()};
  auto n = std::dynamic_pointer_cast<filter_node>(this->node);

  // 0. Unregister all the previous inlets / outlets
  setup.unregister_node_soft(m_oldInlets, m_oldOutlets, node, commands);

  // 1. Recreate ports
  auto [inls, outls] = setup_node(commands);

  // 2. Change the script
  commands.push_back([n, shader = std::make_unique<ProcessedProgram>(shader)] {
    n->set_script(shader->descriptor, shader->vertex, shader->fragment);
  });

  // 3. Register the inlets / outlets
  for(std::size_t i = 0; i < inls.size(); i++)
  {
    setup.register_inlet(*process().inlets()[i], inls[i], node, commands);
  }
  for(std::size_t i = 0; i < outls.size(); i++)
  {
    setup.register_outlet(*process().outlets()[i], outls[i], node, commands);
  }

  commands.run_all();

  m_oldInlets = process().inlets();
  m_oldOutlets = process().outlets();
}

std::pair<ossia::inlets, ossia::outlets>
ISFExecutorComponent::setup_node(Execution::Transaction& commands)
{
  const Execution::Context& ctx = system();
  auto& element = this->process();

  auto n = std::dynamic_pointer_cast<filter_node>(this->node);
  int script_index = ++n->script_index;

  // 1. Create new inlet & outlet arrays
  ossia::inlets inls;
  ossia::outlets outls;

  std::vector<std::shared_ptr<gfx_exec_node::control>> controls;
  std::vector<Execution::ExecutionCommand> controlSetups;

  std::size_t control_index = 0;
  std::weak_ptr<gfx_exec_node> weak_node = n;
  for(auto& ctl : element.inlets())
  {
    QObject::disconnect(ctl, nullptr, this, nullptr);
    if(auto ctrl = qobject_cast<Process::ControlInlet*>(ctl))
    {
      // NOTE! we do not use add_control() here as it changes the internal arrays,
      // while we will replace them after in ossia::recabler

      auto inletport = new ossia::value_inlet;
      auto control = std::make_shared<gfx_exec_node::control>();
      control->port = &**inletport;
      control->changed = true;
      control->value = ctrl->value();
      controls.push_back(control);

      inls.push_back(inletport);
      ctl->setupExecution(*inletport, this);

      // TODO assert that we aren't going to connect twice
      QObject::connect(
          ctrl, &Process::ControlInlet::valueChanged, this,
          con_unvalidated{ctx, control_index, script_index, weak_node});

      control_index++;
    }
    else if([[maybe_unused]] auto ctrl = qobject_cast<Process::AudioInlet*>(ctl))
    {
      inls.push_back(new ossia::audio_inlet);
    }
    else if(auto ctrl = qobject_cast<Gfx::TextureInlet*>(ctl))
    {
      inls.push_back(new ossia::texture_inlet);
      ctrl->setupExecution(*inls.back(), this);
    }
    // else if (auto ctrl = qobject_cast<Gfx::GeometryInlet*>(ctl))
    // {
    //   inls.push_back(new ossia::geometry_inlet);
    // }
  }

  outls.push_back(new ossia::texture_outlet);

  //! TODO the day we have audio outputs in some GFX node
  //! propagate will need to be handled ; right now here
  //! it will cut the sound
  auto recable = std::shared_ptr<ossia::recabler>(
      new ossia::recabler{n, system().execGraph, inls, outls});
  commands.push_back([n, controls, recable]() mutable {
    using namespace std;
    (*recable)();

    swap(n->controls, controls);
  });

  return {std::move(inls), std::move(outls)};
}
}
