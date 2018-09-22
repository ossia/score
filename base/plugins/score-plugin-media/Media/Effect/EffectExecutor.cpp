#include "EffectExecutor.hpp"

#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/port.hpp>
namespace Media
{

EffectProcessComponentBase::EffectProcessComponentBase(
    Media::Effect::ProcessModel& element, const Execution::Context& ctx,
    const Id<score::Component>& id, QObject* parent)
    : Execution::ProcessComponent_T<
          Media::Effect::ProcessModel, ossia::node_chain_process>{
          element, ctx, id, "Executor::EffectComponent", parent}
{
  m_ossia_process = std::make_shared<ossia::node_chain_process>();
  element.effects().orderChanged.connect<&EffectProcessComponentBase::on_orderChanged>(*this);
}

static auto move_edges(
    ossia::inlet& old_in, ossia::inlet_ptr new_in,
    std::shared_ptr<ossia::graph_node> new_node, ossia::graph_interface& g)
{
  auto old_sources = old_in.sources;
  for (auto e : old_sources)
  {
    g.connect(ossia::make_edge(e->con, e->out, new_in, e->out_node, new_node));
    g.disconnect(e);
  }
}
static auto move_edges(
    ossia::outlet& old_out, ossia::outlet_ptr new_out,
    std::shared_ptr<ossia::graph_node> new_node, ossia::graph_interface& g)
{
  auto old_targets = old_out.targets;
  for (auto e : old_targets)
  {
    g.connect(ossia::make_edge(e->con, new_out, e->in, new_node, e->in_node));
    g.disconnect(e);
  }
}

Execution::ProcessComponent* EffectProcessComponentBase::make(
    const Id<score::Component>& id,
    Execution::ProcessComponentFactory& factory, Process::ProcessModel& effect)
{
  if (process().badChaining())
    return nullptr;
  const Execution::Context& ctx = system();
  std::vector<Execution::ExecutionCommand> commands;
  commands.reserve(4);

  std::shared_ptr<ProcessComponent> fx
      = factory.make(effect, system(), id, this);
  if (fx)
  {
    SCORE_ASSERT(fx->node);
    QObject::connect(
        &effect.selection, &Selectable::changed, fx.get(),
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
      m_fxes.insert(m_fxes.begin() + idx, std::make_pair(effect.id(), RegisteredEffect{fx, {}, {}}));
    }

    static_cast<ossia::node_chain_process*>(m_ossia_process.get())
        ->add_node(fx->node);

    auto& this_fx = m_fxes[idx].second;

    SCORE_ASSERT(this_fx.node()->inputs().size() > 0);
    SCORE_ASSERT(this_fx.node()->outputs().size() > 0);
    this_fx.registeredInlets = effect.inlets();
    this_fx.registeredOutlets = effect.outlets();

    // TODO this could be glitchy : there's no guarantee there won't be another
    // tick between all the submitted commands
    if (idx == 0)
    {
      // unregister and re-register previous first node if any
      if (m_fxes.size() > 1)
      {
        auto& old_first = m_fxes[1];
        if (old_first.second)
        {
          unreg(old_first.second);
          // Take all the incoming cables and keep them
          old_first.second.registeredInlets
              = process().effects().at(old_first.first).inlets();
          reg(old_first.second, commands);
        }
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
              move_edges(*n2->inputs()[0], n1->inputs()[0], n1, *g);

              auto edge = ossia::make_edge(
                  ossia::immediate_strict_connection{}, n1->outputs()[0],
                  n2->inputs()[0], n1, n2);
              g->connect(std::move(edge));
            });
      }
      else
      {
        // only effect, goes to end of process
        this_fx.registeredOutlets[0] = process().outlet.get();
        reg(this_fx, commands);

        // set as process node
        auto old = m_ossia_process->node;
        m_ossia_process->node = fx->node;
        nodeChanged(old, m_ossia_process->node);
      }
    }
    else if (idx == (process().effects().size() - 1))
    {
      // unregister previous last node if any
      if (m_fxes.size() > 1)
      {
        auto& old_last = m_fxes[idx - 1];
        if (old_last.second)
        {
          unreg(old_last.second);
          old_last.second.registeredOutlets
              = process().effects().at(old_last.first).outlets();
          reg(old_last.second, commands);
        }
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
          commands.push_back([g = ctx.execGraph, n1 = old_last_comp.second.node(),
                   n2 = this_fx.node()] {
            move_edges(*n1->outputs()[0], n2->outputs()[0], n2, *g);
            auto edge = ossia::make_edge(
                ossia::immediate_strict_connection{}, n1->outputs()[0],
                n2->inputs()[0], n1, n2);
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
      nodeChanged(old, fx->node);
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
              auto& o_prev = n1->outputs()[0];
              if (!o_prev->targets.empty())
              {
                auto cbl = o_prev->targets[0];
                g->disconnect(cbl);
              }

              auto edge = ossia::make_edge(
                  ossia::immediate_strict_connection{}, n1->outputs()[0],
                  n2->inputs()[0], n1, n2);
              g->connect(std::move(edge));
            });
      }

      if (m_fxes.size() > (idx + 1))
      {
        auto& next = m_fxes[idx + 1];

        if (next.second)
        {
          commands.push_back([g = ctx.execGraph, n2 = this_fx.node(),
                   n3 = next.second.node()] {
            auto& i_next = n3->inputs()[0];
            if (!i_next->sources.empty())
            {
              auto cbl = i_next->sources[0];
              g->disconnect(cbl);
            }
            auto edge = ossia::make_edge(
                ossia::immediate_strict_connection{}, n2->outputs()[0],
                n3->inputs()[0], n2, n3);
            g->connect(std::move(edge));
          });
        }
      }
    }
  }

  if(!commands.empty())
  {
    in_exec([f=std::move(commands)] { for(auto& cmd : f) { cmd(); } });
  }
  return fx.get();
}

void EffectProcessComponentBase::added(ProcessComponent& e)
{
}

std::function<void()> EffectProcessComponentBase::removing(
    const Process::ProcessModel& e, ProcessComponent& c)
{
  if (process().badChaining())
    return {};

  std::vector<Execution::ExecutionCommand> commands;
  auto echain
      = std::dynamic_pointer_cast<ossia::node_chain_process>(m_ossia_process);

  auto it = ossia::find_if(
      m_fxes, [&](const auto& v) { return v.first == e.id(); });
  if (it == m_fxes.end())
    return {};
  std::size_t idx = std::distance(m_fxes.begin(), it);
  auto& this_fx = it->second;

  // Remove all the chaining
  commands.push_back([g = system().execGraph, n = this_fx.node(), echain] {
    ossia::remove_one(echain->nodes, n);
    n->clear();
    g->remove_node(n);
  });

  unreg(this_fx);

  if (idx == 0)
  {
    if (m_fxes.size() > 1)
    {
      // Link start with next idx
      auto& new_first = m_fxes[1];
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
  }
  else if (idx == (m_fxes.size() - 1))
  {
    if (m_fxes.size() > 1)
    {
      auto& new_last = m_fxes[idx - 1];
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
          commands.push_back([proc = m_ossia_process, node = new_last.second.node()] {
            proc->node = node;
          });
          nodeChanged(old, new_last.second.node());
        }
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
        commands.push_back([g = system().execGraph, n1 = prev.second.node(),
                 n2 = next.second.node()] {
          auto edge = ossia::make_edge(
              ossia::immediate_strict_connection{}, n1->outputs()[0],
              n2->inputs()[0], n1, n2);
          g->connect(std::move(edge));
        });
      }
    }
  }

  if(!commands.empty())
  {
    in_exec([f=std::move(commands)] { for(auto& cmd : f) { cmd(); } });
  }

  this_fx.comp->node.reset();
  return [=] { m_fxes.erase(it); };
}

void EffectProcessComponentBase::on_orderChanged()
{

}

void EffectProcessComponentBase::unreg(
    const EffectProcessComponentBase::RegisteredEffect& fx)
{
  system().setup.unregister_node_soft(
        fx.registeredInlets, fx.registeredOutlets, fx.node());
}

void EffectProcessComponentBase::reg(
    const RegisteredEffect& fx, std::vector<Execution::ExecutionCommand>& vec)
{
  system().setup.register_node(
      fx.registeredInlets, fx.registeredOutlets, fx.node(), vec);
}

void EffectProcessComponent::cleanup()
{
  clear();
  ProcessComponent::cleanup();
}

EffectProcessComponentBase::~EffectProcessComponentBase()
{
}

EffectProcessComponent::~EffectProcessComponent()
{
}
}
