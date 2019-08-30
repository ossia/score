#include "Executor.hpp"

#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/node_chain_process.hpp>

#include <Nodal/Process.hpp>
#include <score/document/DocumentContext.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <score/application/ApplicationContext.hpp>
#include <ossia/detail/flat_set.hpp>

namespace ossia
{

struct node_graph_process final : public ossia::time_process
{
  node_graph_process()
  {
    m_lastDate = ossia::Zero;
  }

  void state_impl(
      ossia::time_value from, ossia::time_value to, ossia::time_value parent_duration,
      ossia::time_value tick_offset, double speed) override
  {
    for (auto& process : processes)
    {
      process->state(from, to, parent_duration, tick_offset, speed);
    }
    m_lastDate = to;
  }

  void add_process(std::shared_ptr<ossia::time_process>&& p, std::shared_ptr<ossia::graph_node>&& n)
  {
    nodes.insert(std::move(n));
    processes.insert(std::move(p));
  }


  void remove_process(const std::shared_ptr<ossia::time_process>& p, const std::shared_ptr<ossia::graph_node>& n)
  {
    nodes.erase(n);
    processes.erase(p);
  }
  void start() override
  {
    for (auto& process : processes)
    {
      process->start();
    }
  }

  void stop() override
  {
    for (auto& process : processes)
    {
      process->stop();
    }
    for (auto& node : nodes)
    {
      node->all_notes_off();
    }
  }

  void pause() override
  {
    for (auto& process : processes)
    {
      process->pause();
    }
  }

  void resume() override
  {
    for (auto& process : processes)
    {
      process->resume();
    }
  }

  void offset_impl(time_value date, double pos) override
  {
    for (auto& process : processes)
    {
      process->offset(date, pos);
    }
    for (auto& node : nodes)
    {
      node->all_notes_off();
    }
  }

  void transport_impl(ossia::time_value date, double pos) override
  {
    for (auto& process : processes)
    {
      process->transport(date, pos);
    }
    for (auto& node : nodes)
    {
      node->all_notes_off();
    }
  }

  void mute_impl(bool b) override
  {
    for (auto& node : nodes)
      node->set_mute(b);
  }
  ossia::flat_set<std::shared_ptr<ossia::graph_node>> nodes;
  ossia::flat_set<std::shared_ptr<ossia::time_process>> processes;
  ossia::time_value m_lastDate{ossia::Infinite};

};
}
namespace Nodal
{


NodalExecutorBase::NodalExecutorBase(
    Nodal::Model& element, const Execution::Context& ctx,
    const Id<score::Component>& id, QObject* parent)
    : ProcessComponent_T{element, ctx, id, "NodalExecutorComponent", parent}
{
  // TODO load node
  m_ossia_process = std::make_shared<ossia::node_graph_process>();
}

NodalExecutorBase::~NodalExecutorBase()
{

}

void NodalExecutorBase::unreg(
    const RegisteredNode& fx)
{
  system().setup.unregister_node_soft(
      fx.comp->process().inlets(), fx.comp->process().outlets(), fx.comp->node);
}

void NodalExecutorBase::reg(
    const RegisteredNode& fx,
    std::vector<Execution::ExecutionCommand>& vec)
{
  system().setup.register_node(
              fx.comp->process().inlets(), fx.comp->process().outlets(), fx.comp->node, vec);
}


Execution::ProcessComponent* NodalExecutorBase::make(
    const Id<score::Component>& id,
    Execution::ProcessComponentFactory& factory,
    Nodal::Node& node)
{
  std::vector<Execution::ExecutionCommand> commands;
  auto& proc = node.process();
  auto comp = factory.make(proc, this->system(), id, this);
  if (comp)
  {
    reg(m_nodes[node.id()] = {comp}, commands);
    auto child_n = comp->node;
    auto child_p = comp->OSSIAProcessPtr();
    if(child_n && child_p)
    {
      auto p = std::dynamic_pointer_cast<ossia::node_graph_process>(m_ossia_process);
      commands.push_back([child_n=std::move(child_n), child_p=std::move(child_p), p=std::move(p)] () mutable
      {
        p->add_process(std::move(child_p), std::move(child_n));
      });
    }

    // TODO memory should be brought back in the main thread to be freed
    in_exec([f = std::move(commands)] {
      for (auto& cmd : f)
        cmd();
    });
  }

  return comp.get();
}

void NodalExecutorBase::added(::Execution::ProcessComponent& e)
{

}

std::function<void()> NodalExecutorBase::removing(
    const Nodal::Node& e,
    ::Execution::ProcessComponent& c)
{
  std::vector<Execution::ExecutionCommand> commands;

  auto it = ossia::find_if(
      m_nodes, [&](const auto& v) { return v.first == e.id(); });
  if (it == m_nodes.end())
    return {};

  auto& this_fx = it->second;

  unreg(this_fx);

  auto p = std::dynamic_pointer_cast<ossia::node_graph_process>(m_ossia_process);
  auto child_p = c.OSSIAProcessPtr();
  auto child_n = c.node;
  commands.push_back(
     [child_n=std::move(child_n), child_p=std::move(child_p), p=std::move(p)]
  {
    p->remove_process(child_p, child_n);
  });

  // TODO add a "exec all commands macro
  in_exec([f = std::move(commands)] {
    for (auto& cmd : f)
      cmd();
  });

  c.node.reset();
  return {};
}

void NodalExecutor::cleanup()
{
  clear();
  ProcessComponent::cleanup();
}

NodalExecutor::~NodalExecutor()
{

}

}
