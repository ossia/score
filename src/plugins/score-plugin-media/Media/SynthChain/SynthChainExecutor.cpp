#include "SynthChainExecutor.hpp"

#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Media/ChainExecutor.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/nodes/dummy.hpp>

#include <fmt/format.h>
#include <QDebug>

namespace Media
{
SynthChainComponentBase::SynthChainComponentBase(
    Media::SynthChain::ProcessModel& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : Execution::ProcessComponent_T<
          Media::SynthChain::ProcessModel,
          ossia::node_chain_process>{element,
                                     ctx,
                                     id,
                                     "Executor::EffectComponent",
                                     parent}
{
  m_ossia_process = std::make_shared<ossia::node_chain_process>();
  element.effects()
      .orderChanged.connect<&SynthChainComponentBase::on_orderChanged>(
          *this);
}

SynthChainComponentBase::~SynthChainComponentBase()
{
  process()
      .effects()
      .orderChanged.disconnect<&SynthChainComponentBase::on_orderChanged>(
          *this);
}

void SynthChainComponentBase::unregister_old_first_node(
    std::pair<
        Id<Process::ProcessModel>,
        SynthChainComponentBase::RegisteredEffect>& old_first,
    Execution::Transaction& commands)
{
  if (old_first.second)
  {
    unreg(old_first.second);
    // Take all the incoming cables and keep them
    old_first.second.registeredInlets
        = process().effects().at(old_first.first).inlets();
    reg(old_first.second, commands);
  }
}
void SynthChainComponentBase::register_new_first_node(
    std::pair<
        Id<Process::ProcessModel>,
        SynthChainComponentBase::RegisteredEffect>& new_first,
    Execution::Transaction& commands)
{
  if (new_first.second)
  {
    unreg(new_first.second);
    if (new_first.second.node())
    {
      new_first.second.registeredInlets
          = process().effects().at(new_first.first).inlets();
      new_first.second.registeredInlets[0] = process().inlet.get();
      reg(new_first.second, commands);
    }
  }
}

void SynthChainComponentBase::unregister_old_last_node(
    std::pair<
        Id<Process::ProcessModel>,
        SynthChainComponentBase::RegisteredEffect>& old_last,
    Execution::Transaction& commands)
{
  if (old_last.second)
  {
    unreg(old_last.second);
    old_last.second.registeredOutlets
        = process().effects().at(old_last.first).outlets();
    reg(old_last.second, commands);
  }
}
void SynthChainComponentBase::register_new_last_node(
    std::pair<
        Id<Process::ProcessModel>,
        SynthChainComponentBase::RegisteredEffect>& new_last,
    Execution::Transaction& commands)
{
  if (new_last.second)
  {
    unreg(new_last.second);
    if (new_last.second.node())
    {
      new_last.second.registeredOutlets
          = process().effects().at(new_last.first).outlets();
      new_last.second.registeredOutlets[0] = process().outlet.get();
      reg(new_last.second, commands);

      auto old = m_ossia_process->node;
      commands.push_back(
          [proc = m_ossia_process, node = new_last.second.node()] {
            proc->node = node;
          });
      nodeChanged(old, new_last.second.node(), commands);
    }
  }
}
Execution::ProcessComponent* SynthChainComponentBase::make(
    const Id<score::Component>& id,
    Execution::ProcessComponentFactory& factory,
    Process::ProcessModel& effect)
{
  if (process().badChaining())
    return nullptr;
  const Execution::Context& ctx = system();
  const auto& echain
      = std::dynamic_pointer_cast<ossia::node_chain_process>(m_ossia_process);
  Execution::Transaction commands{system()};
  commands.reserve(4);

  std::shared_ptr<ProcessComponent> fx
      = factory.make(effect, system(), id, this);
  if (!fx)
  {
    fx = std::make_shared<DummyProcessComponent>(effect, system(), id, this);
  }

  SCORE_ASSERT(fx->node);
  QObject::connect(
        &effect.selection,
        &Selectable::changed,
        fx.get(),
        [this, n = fx->node](bool ok) {
    in_exec([=] { n->set_logging(ok); });
  });

  const auto idx_ = process().effectPosition(effect.id());
  SCORE_ASSERT(idx_ != -1);
  const std::size_t idx = idx_;
  if (m_fxes.size() < (idx + 1))
  {
    m_fxes.resize(idx + 1);
    m_fxes[idx] = std::make_pair(effect.id(), RegisteredEffect{fx, {}, {}});
  }
  else
  {
    m_fxes.insert(
          m_fxes.begin() + idx,
          std::make_pair(effect.id(), RegisteredEffect{fx, {}, {}}));
  }

  commands.push_back(
        [idx, proc = echain, n = fx->node] { proc->add_node(idx, n); });

  auto& this_fx = m_fxes[idx].second;

  SCORE_ASSERT(this_fx.node()->root_inputs().size() > 0);
  SCORE_ASSERT(this_fx.node()->root_outputs().size() > 0);
  this_fx.registeredInlets = effect.inlets();
  this_fx.registeredOutlets = effect.outlets();

  if (idx == 0)
  {
    // unregister and re-register previous first node if any
    if (m_fxes.size() > 1)
    {
      unregister_old_first_node(m_fxes[1], commands);
    }

    // register new first node ; first inlet is process's ; last inlets are
    // node's
    this_fx.registeredInlets[0] = process().inlet.get();

    // connect out to next in
    if (m_fxes.size() > 1)
    {
      // there's an effect after
      reg(this_fx, commands);
      commands.push_back(
            [g = ctx.execGraph, n1 = fx->node, n2 = m_fxes[1].second.node()] {
        move_edges(*n2->root_inputs()[0], n1->root_inputs()[0], n1, *g);

        auto edge = ossia::make_edge(
              ossia::immediate_strict_connection{},
              n1->root_outputs()[0],
            n2->root_inputs()[0],
            n1,
            n2);
        g->connect(std::move(edge));
      });
    }
    else
    {
      auto old = m_passthrough;
      if(old)
      {
        commands.push_back([
                           g = ctx.execGraph,
                           echain,
                           proc = m_ossia_process,
                           pt = old,
                           new_node = fx->node] () mutable {
          // passthrough has been put in position 2
          // by proc->add_node(idx, n);
          // so we remove it
          echain->nodes.resize(1);
          proc->node = new_node;

          if(!pt->root_inputs().empty() && !new_node->root_inputs().empty())
            move_edges(*pt->root_inputs()[0], new_node->root_inputs()[0], new_node, *g);
          // no need to copy outs : done in IntervalExecution (c.f. nodeChanged signal)
        });

        system().context().setup.unregister_node(process().inlets(), process().outlets(), old, commands);
        m_passthrough.reset();
      }
      else {
        commands.push_back([
                           proc = m_ossia_process,
                           new_node = fx->node] () mutable {
          proc->node = new_node;
        });
      }

      // only effect, goes to end of process
      this_fx.registeredOutlets[0] = process().outlet.get();
      reg(this_fx, commands);

      // set as process node
      nodeChanged(old, fx->node, commands);
    }
  }
  else if (idx == (process().effects().size() - 1))
  {
    // unregister previous last node if any
    if (idx >= 1)
    {
      unregister_old_last_node(m_fxes[idx - 1], commands);
    }

    // register new last node
    this_fx.registeredOutlets[0] = process().outlet.get();

    // connect in to prev out
    if (m_fxes.size() > 1)
    {
      reg(this_fx, commands);

      // there's an effect before
      auto& old_last_comp = m_fxes[idx - 1];
      if (old_last_comp.second)
      {
        commands.push_back([g = ctx.execGraph,
                           n1 = old_last_comp.second.node(),
                           n2 = this_fx.node()] {
          // unnecessary : done in IntervalExecution (c.f. nodeChanged signal)
          //move_edges(*n1->root_outputs()[0], n2->root_outputs()[0], n2, *g);
          auto edge = ossia::make_edge(
                ossia::immediate_strict_connection{},
                n1->root_outputs()[0],
              n2->root_inputs()[0],
              n1,
              n2);
          g->connect(std::move(edge));
        });
      }
    }
    else
    {
      // only effect
      this_fx.registeredInlets[0] = process().inlet.get();
      reg(this_fx, commands);
    }

    // set as process node
    auto old = m_ossia_process->node;
    commands.push_back(
          [proc = m_ossia_process, node = fx->node] { proc->node = node; });
    nodeChanged(old, fx->node, commands);
  }
  else
  {
    // register node with effect's inlets
    reg(this_fx, commands);

    // unlink before and after and link this one in-between
    auto& prev = m_fxes[idx - 1];
    if (prev.second)
    {
      commands.push_back(
            [g = ctx.execGraph, n1 = prev.second.node(), n2 = this_fx.node()] {
        auto& o_prev = n1->root_outputs()[0];
        if (!o_prev->targets.empty())
        {
          auto cbl = o_prev->targets[0];
          g->disconnect(cbl);
        }

        auto edge = ossia::make_edge(
              ossia::immediate_strict_connection{},
              n1->root_outputs()[0],
            n2->root_inputs()[0],
            n1,
            n2);
        g->connect(std::move(edge));
      });
    }

    if (m_fxes.size() > (idx + 1))
    {
      auto& next = m_fxes[idx + 1];

      if (next.second)
      {
        commands.push_back([g = ctx.execGraph,
                           n2 = this_fx.node(),
                           n3 = next.second.node()] {
          auto& i_next = n3->root_inputs()[0];
          if (!i_next->sources.empty())
          {
            auto cbl = i_next->sources[0];
            g->disconnect(cbl);
          }
          auto edge = ossia::make_edge(
                ossia::immediate_strict_connection{},
                n2->root_outputs()[0],
              n3->root_inputs()[0],
              n2,
              n3);
          g->connect(std::move(edge));
        });
      }
    }
  }

  if (!commands.empty())
  {
#if !defined(NDEBUG)
    in_exec([f = std::move(commands),
             g = ctx.execGraph,
             proc = echain,
             test_fx = get_nodes(m_fxes)] () mutable {
      f.run_all_in_exec();
      check_exec_validity(*g, *proc);
      check_exec_order(test_fx, *proc);
    });
#else
    commands.run_all();
#endif
  }
  return fx.get();
}

void SynthChainComponentBase::added(ProcessComponent& e)
{

}

std::function<void()> SynthChainComponentBase::removing(
    const Process::ProcessModel& e,
    ProcessComponent& c)
{
  if (process().badChaining())
    return {};

  const Execution::Context& ctx = system();

  Execution::Transaction commands{ctx};

  auto it = ossia::find_if(
      m_fxes, [&](const auto& v) { return v.first == e.id(); });
  if (it == m_fxes.end())
    return {};
  std::size_t idx = std::distance(m_fxes.begin(), it);
  auto& this_fx = it->second;

  auto old_node_in = this_fx.node()->root_inputs();
  auto old_node_out = this_fx.node()->root_outputs();
  // Remove all the chaining
  auto echain
      = std::dynamic_pointer_cast<ossia::node_chain_process>(m_ossia_process);

  unreg(this_fx);

  if (idx == 0)
  {
    if (m_fxes.size() > 1)
    {
      // Link start with next idx
      register_new_first_node(m_fxes[1], commands);
      commands.push_back(
          [g = ctx.execGraph, n1 = m_fxes[1].second.node(), n2 = this_fx.node()] {
        if(!n2->root_inputs().empty() && !n1->root_inputs().empty())
            move_edges(*n2->root_inputs()[0], n1->root_inputs()[0], n1, *g);
      });
    }
    else if (m_fxes.size() == 1)
    {
      createPassthrough(commands);
      if(m_passthrough && this_fx.node())
      {
        commands.push_back(
              [g = ctx.execGraph, n1 = m_passthrough, n2 = this_fx.node()] {
          if(!n2->root_inputs().empty() && !n1->root_inputs().empty())
            move_edges(*n2->root_inputs()[0], n1->root_inputs()[0], n1, *g);
          if(!n2->root_outputs().empty() && !n1->root_outputs().empty())
            move_edges(*n2->root_outputs()[0], n1->root_outputs()[0], n1, *g);
        });
      }
    }
  }
  else if (idx == (m_fxes.size() - 1))
  {
    if (m_fxes.size() > 1)
    {
      register_new_last_node(m_fxes[idx - 1], commands);
      commands.push_back(
          [g = ctx.execGraph, n1 = m_fxes[idx - 1].second.node(), n2 = this_fx.node()] {
        if(!n2->root_outputs().empty() && !n1->root_outputs().empty())
            move_edges(*n2->root_outputs()[0], n1->root_outputs()[0], n1, *g);
      });
    }
    else if (m_fxes.size() == 1)
    {
      createPassthrough(commands);
      if(m_passthrough && this_fx.node())
      {
        commands.push_back(
              [g = ctx.execGraph, n1 = m_passthrough, n2 = this_fx.node()] {
          if(!n2->root_inputs().empty() && !n1->root_inputs().empty())
            move_edges(*n2->root_inputs()[0], n1->root_inputs()[0], n1, *g);
          if(!n2->root_outputs().empty() && !n1->root_outputs().empty())
            move_edges(*n2->root_outputs()[0], n1->root_outputs()[0], n1, *g);
        });
      }
    }
  }
  else
  {
    if (m_fxes.size() > (idx + 1))
    {
      auto& prev = m_fxes[idx - 1];
      auto& next = m_fxes[idx + 1];
      if (prev.second && next.second)
      {
        commands.push_back([g = system().execGraph,
                            n1 = prev.second.node(),
                            n2 = next.second.node()] {
          auto edge = ossia::make_edge(
              ossia::immediate_strict_connection{},
              n1->root_outputs()[0],
              n2->root_inputs()[0],
              n1,
              n2);
          g->connect(std::move(edge));
        });
      }
    }
    else {
      qDebug() << "m_fxes.size() <= (idx + 1)" << m_fxes.size() << (idx + 1);
    }
  }

  commands.push_back([g = system().execGraph, n = this_fx.node(), echain] {
    ossia::remove_one(echain->nodes, n);
    if (echain->nodes.empty())
      echain->node.reset();
    n->clear();
    g->remove_node(n);
  });

#if !defined(NDEBUG)
  auto test_fx = get_nodes(m_fxes);
  if(m_passthrough)
    test_fx = {m_passthrough};
  ossia::remove_erase(test_fx, this_fx.node());
  in_exec([f = std::move(commands),
          g = ctx.execGraph,
          proc = echain,
          test_fx,
          clearing = m_clearing] () mutable {
    f.run_all_in_exec();
    if (!clearing)
    {
      check_exec_validity(*g, *proc);
      check_last_validity(*g, *proc);
      check_exec_order(test_fx, *proc);
    }
  });
#else
  commands.run_all();
#endif

  this_fx.comp->node.reset();
  return [=] { m_fxes.erase(it); };
}

void SynthChainComponentBase::createPassthrough(Execution::Transaction& commands)
{
  if(m_passthrough)
    return;
  if(process().effects().size() > 1)
    return;
  m_passthrough = std::make_shared<ossia::empty_audio_mapper>();
  auto old_node = this->node;
  this->node = m_passthrough;

  auto echain = std::dynamic_pointer_cast<ossia::node_chain_process>(m_ossia_process);
  commands.push_back([echain, proc = m_ossia_process, pt = m_passthrough] () mutable {
    echain->add_node(0, pt);
    proc->node = std::move(pt);
  });
  system().context().setup.register_node(process().inlets(), process().outlets(), m_passthrough, commands);
  nodeChanged(old_node, this->node, commands);
}

void SynthChainComponentBase::on_orderChanged()
{
  if (process().badChaining())
  {
    // TODO how to handle this ?
    return;
  }

  if (process().effects().empty() || m_fxes.empty())
    return;

  // TODO it would be less expensive to swap things starting from i = 0
  std::vector<std::pair<Id<Process::ProcessModel>, RegisteredEffect>> fxes;
  std::vector<std::shared_ptr<ossia::graph_node>> nodes;

  for (auto& f : process().effects())
  {
    auto it = ossia::find_if(
        m_fxes, [&](const auto& pair) { return pair.first == f.id(); });
    SCORE_ASSERT(it != m_fxes.end());

    fxes.push_back(*it);
    if (const auto& comp = it->second.comp; comp->node)
    {
      nodes.push_back(comp->node);
    }
  }
  auto old_first = m_fxes.front();
  bool first_changed = (fxes.front() != m_fxes.front());

  auto old_last = m_fxes.back();
  bool last_changed = (fxes.back() != m_fxes.back());

  m_fxes = std::move(fxes);

  const Execution::Context& ctx = system();

  Execution::Transaction commands{ctx};

  if (first_changed && old_first.second.node())
  {
    unreg(old_first.second);
    old_first.second.registeredInlets
        = process().effects().at(old_first.first).inlets();
    reg(old_first.second, commands);

    register_new_first_node(m_fxes.front(), commands);
  }

  if (last_changed && old_last.second.node())
  {
    unreg(old_last.second);
    old_last.second.registeredOutlets
        = process().effects().at(old_last.first).outlets();
    reg(old_last.second, commands);

    register_new_last_node(m_fxes.back(), commands);
  }

  const auto& echain
      = std::dynamic_pointer_cast<ossia::node_chain_process>(m_ossia_process);
  commands.push_back(
      [g = ctx.execGraph, proc = echain, f = std::move(nodes)]() mutable {
        // 1. Disconnect existing cables
        for (auto it = proc->nodes.begin(); it != proc->nodes.end(); ++it)
        {
          if (it + 1 == proc->nodes.end())
            break;
          auto cable = it->get()->root_outputs()[0]->targets[0];
          g->disconnect(cable);
        }

        // 2. Change node order
        proc->nodes = std::move(f);

        // 3. Connect new cables
        for (auto it = proc->nodes.begin(); it != proc->nodes.end(); ++it)
        {
          if (it + 1 == proc->nodes.end())
            break;
          const auto& n1 = *it;
          const auto& n2 = *(it + 1);
          auto edge = ossia::make_edge(
              ossia::immediate_strict_connection{},
              n1->root_outputs()[0],
              n2->root_inputs()[0],
              n1,
              n2);
          g->connect(std::move(edge));
        }
      });

  if (!commands.empty())
  {
#if !defined(NDEBUG)
    in_exec([f = std::move(commands),
             g = ctx.execGraph,
             proc = echain,
             test_fx = get_nodes(m_fxes)] () mutable {
      f.run_all_in_exec();

      check_exec_validity(*g, *proc);
      check_last_validity(*g, *proc);
      check_exec_order(test_fx, *proc);
    });
#else
    commands.run_all();
#endif
  }
}

void SynthChainComponentBase::unreg(
    const SynthChainComponentBase::RegisteredEffect& fx)
{
  system().setup.unregister_node_soft(
      fx.registeredInlets, fx.registeredOutlets, fx.node());
}

void SynthChainComponentBase::reg(
    const RegisteredEffect& fx,
    Execution::Transaction& vec)
{
  system().setup.register_node(
      fx.registeredInlets, fx.registeredOutlets, fx.node(), vec);
}

SynthChainComponent::SynthChainComponent(
    SynthChain::ProcessModel& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id, QObject* parent)
  : score::PolymorphicComponentHierarchy<
    SynthChainComponentBase,
    false>{score::lazy_init_t{}, element, ctx, id, parent}
{
  if (!element.badChaining())
    init_hierarchy();

  connect(
        &element,
        &Media::SynthChain::ProcessModel::badChainingChanged,
        this,
        [&](bool b) {
    if (b)
    {
      clear();
    }
    else
    {
      init_hierarchy();
    }
  });

  if(element.effects().empty())
  {
    Execution::Transaction commands{ctx};
    createPassthrough(commands);
    commands.run_all();
  }
}

void SynthChainComponent::cleanup()
{
#if !defined(NDEBUG)
  m_clearing = true;
#endif
  clear();
  ProcessComponent::cleanup();
}

SynthChainComponent::~SynthChainComponent() {}
}
