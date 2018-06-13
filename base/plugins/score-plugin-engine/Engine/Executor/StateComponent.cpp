// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StateComponent.hpp"

#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/nodes/state.hpp>
#include <ossia/editor/scenario/time_event.hpp>

#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/score2OSSIA.hpp>
#include <ossia/detail/pod_vector.hpp>

namespace Engine
{
namespace Execution
{
StateComponentBase::StateComponentBase(
    const Scenario::StateModel& element,
    const Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : Execution::Component{ctx, id, "Executor::State", nullptr}
    , m_model{element}
    , m_node{std::make_shared<ossia::nodes::state_writer>(Engine::score_to_ossia::state(element, ctx))}
{
  connect(&element, &Scenario::StateModel::sig_statesUpdated,
          this, [this, &ctx] {
    in_exec(
          [
          n=m_node
         ,x=Engine::score_to_ossia::state(m_model, ctx)
          ] () mutable {
      n->data = std::move(x);
    });
  });
}

void StateComponentBase::onDelete() const
{
  system().plugin.unregister_node({}, {}, m_node);
  in_exec([gr = this->system().plugin.execGraph, ev = m_ev] {
    auto& procs = ev->get_time_processes();
    if (!procs.empty())
    {
      const auto& proc = (*procs.begin());
      ev->remove_time_process(proc.get());
    }
  });
}

ProcessComponent* StateComponentBase::make(
    const Id<score::Component>& id,
    ProcessComponentFactory& fac,
    Process::ProcessModel& proc)
{
  try
  {
    const Engine::Execution::Context& ctx = system();
    auto plug = fac.make(proc, ctx, id, nullptr);
    if (plug && plug->OSSIAProcessPtr())
    {
      auto oproc = plug->OSSIAProcessPtr();
      m_processes.emplace(proc.id(), plug);

      const auto& outlets = proc.outlets();
      ossia::pod_vector<std::size_t> propagated_outlets;
      for (std::size_t i = 0; i < outlets.size(); i++)
      {
        if (outlets[i]->propagate())
          propagated_outlets.push_back(i);
      }

      if (auto& onode = plug->node)
        ctx.plugin.register_node(proc, onode);

      auto cst = m_ev;

      QObject::connect(
          &proc.selection, &Selectable::changed, plug.get(),
          [this, n = oproc->node](bool ok) {
            in_exec([=] {
              if (n)
                n->set_logging(ok);
            });
          });
      if (oproc->node)
        oproc->node->set_logging(proc.selection.get());

      std::weak_ptr<ossia::time_process> oproc_weak = oproc;
      std::weak_ptr<ossia::graph_interface> g_weak
          = plug->system().plugin.execGraph;

      in_exec([cst = m_ev, oproc_weak, g_weak, propagated_outlets] {
        if (auto oproc = oproc_weak.lock())
          if (auto g = g_weak.lock())
          {
            cst->add_time_process(oproc);
          }
      });

      return plug.get();
    }
  }
  catch (const std::exception& e)
  {
    qDebug() << "Error while creating a process: " << e.what();
  }
  catch (...)
  {
    qDebug() << "Error while creating a process";
  }
  return nullptr;
}

std::function<void()> StateComponentBase::removing(
    const Process::ProcessModel& e, ProcessComponent& c)
{
  auto it = m_processes.find(e.id());
  if (it != m_processes.end())
  {
    auto c_ptr = c.shared_from_this();
    in_exec([cstr = m_ev, c_ptr] {
      cstr->remove_time_process(c_ptr->OSSIAProcessPtr().get());
    });
    c.cleanup();

    return [=] { m_processes.erase(it); };
  }

  return {};
}

StateComponent::~StateComponent()
{
}

void StateComponent::onSetup(
    const std::shared_ptr<ossia::time_event>& root)
{
  m_ev = root;
  m_ev->add_time_process(std::make_shared<ossia::node_process>(m_node));
  system().plugin.register_node({}, {}, m_node);

  init();
}

void StateComponent::init()
{
  init_hierarchy();
}


void StateComponent::cleanup(const std::shared_ptr<StateComponent>& self)
{
  if (m_ev)
  {
    // self has to be kept alive until next tick
    in_exec([itv = m_ev, self] { itv->cleanup(); });
  }
  for (auto& proc : m_processes)
    proc.second->cleanup();

  clear();
  m_processes.clear();
  m_ev.reset();
  m_node.reset();
  disconnect();
}
}
}
