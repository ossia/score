#include "EffectExecutor.hpp"

namespace Engine
{
namespace Execution
{

EffectProcessComponentBase::EffectProcessComponentBase(
    Media::Effect::ProcessModel &element,
    const Engine::Execution::Context &ctx,
    const Id<score::Component> &id,
    QObject *parent)
  : Engine::Execution::ProcessComponent_T<Media::Effect::ProcessModel, effect_chain_process>{
      element,
      ctx,
      id, "Executor::EffectComponent", parent}
{
  m_ossia_process = std::make_shared<effect_chain_process>();
}

EffectComponent* EffectProcessComponentBase::make(
    const Id<score::Component>& id,
    EffectComponentFactory& factory,
    Media::Effect::EffectModel& effect)
{
  Media::Effect::ProcessModel& element = process();
  const Engine::Execution::Context & ctx = system();

  std::shared_ptr<EffectComponent> fx = factory.make(effect, system(), id, this);
  if(fx)
  {
    SCORE_ASSERT(fx->node);
    auto idx = process().effectPosition(effect.id());
    SCORE_ASSERT(idx != -1);
    if(m_fxes.size() < (idx + 1))
      m_fxes.resize(idx + 1);
    m_fxes[idx] = std::make_pair(effect.id(), RegisteredEffect{fx, {}, {}});
    static_cast<effect_chain_process*>(m_ossia_process.get())->nodes.push_back(fx->node);

    auto unreg = [&] (const RegisteredEffect& fx) {
      system().plugin.unregister_node_soft(fx.registeredInlets, fx.registeredOutlets, fx.node());
    };
    auto reg = [&] (const RegisteredEffect& fx) {
      system().plugin.register_node(fx.registeredInlets, fx.registeredOutlets, fx.node());
    };
    auto& this_fx = m_fxes[idx].second;

    SCORE_ASSERT(this_fx.node()->inputs().size() > 0);
    SCORE_ASSERT(this_fx.node()->outputs().size() > 0);
    this_fx.registeredInlets = effect.inlets();
    this_fx.registeredOutlets = effect.outlets();
    // TODO this could be glitchy : there's no guarantee there won't be another tick between all the submitted commands
    if(idx == 0)
    {
      // unregister and re-register previous first node if any
      if(m_fxes.size() > 1)
      {
        auto& old_first = m_fxes[1];
        if(old_first.second)
        {
          unreg(old_first.second);
          old_first.second.registeredInlets = process().effects().at(old_first.first).inlets();
          reg(old_first.second);
        }
      }

      // register new first node ; first inlet is process's ; last inlets are node's
      this_fx.registeredInlets[0] = process().inlet.get();

      // connect out to next in
      if(m_fxes.size() > 1)
      {
        // there's an effect after
        reg(this_fx);
        system().executionQueue.enqueue(
              [g=ctx.plugin.execGraph, n1=fx->node, n2=m_fxes[1].second.node()]
        {
          auto edge = ossia::make_edge(
            ossia::immediate_strict_connection{}, n1->outputs()[0], n2->inputs()[0], n1, n2);
          g->connect(std::move(edge));
        });
      }
      else
      {
        // only effect, goes to end of process
        this_fx.registeredOutlets[0] = process().outlet.get();
        reg(this_fx);

        // set as process node
        auto old = m_ossia_process->node;
        m_ossia_process->node = fx->node;
        emit nodeChanged(old, m_ossia_process->node);
      }
    }
    else if(idx == (process().effects().size() - 1))
    {
      // unregister previous last node if any
      if(m_fxes.size() > 1)
      {
        auto& old_last = m_fxes[idx-1];
        if(old_last.second)
        {
          unreg(old_last.second);
          old_last.second.registeredOutlets = process().effects().at(old_last.first).outlets();
          reg(old_last.second);
        }
      }

      // register new last node
      this_fx.registeredOutlets[0] = process().outlet.get();

      // connect in to prev out
      if(m_fxes.size() > 1)
      {
        reg(this_fx);

        // there's an effect before
        auto& old_last_comp = m_fxes[idx-1];
        if(old_last_comp.second)
        {
          system().executionQueue.enqueue(
                [g=ctx.plugin.execGraph, n1=old_last_comp.second.node(), n2=this_fx.node()]
          {
            auto edge = ossia::make_edge(
                          ossia::immediate_strict_connection{}, n1->outputs()[0], n2->inputs()[0], n1, n2);
            g->connect(std::move(edge));
          });
        }
      }
      else
      {
        // only effect
        this_fx.registeredInlets[0] = process().inlet.get();
        reg(this_fx);
      }

      // set as process node
      auto old = m_ossia_process->node;
      m_ossia_process->node = fx->node;
      emit nodeChanged(old, m_ossia_process->node);
    }
    else
    {
      // register node with effect's inlets
      reg(this_fx);

      // unlink before and after and link this one in-between

      auto& prev = m_fxes[idx-1];
      if(prev.second)
      {
        system().executionQueue.enqueue(
              [g=ctx.plugin.execGraph, n1=prev.second.node(), n2=this_fx.node()] {
          auto& o_prev = n1->outputs()[0];
          if(!o_prev->targets.empty())
          {
            auto cbl = o_prev->targets[0];
            g->disconnect(cbl);
          }

          auto edge = ossia::make_edge(
                        ossia::immediate_strict_connection{}, n1->outputs()[0], n2->inputs()[0], n1, n2);
          g->connect(std::move(edge));
        });
      }

      if(m_fxes.size() > (idx+1))
      {
        auto& next = m_fxes[idx+1];

        if(next.second)
        {
          system().executionQueue.enqueue(
                [g=ctx.plugin.execGraph, n2=this_fx.node(), n3=next.second.node()] {
            auto& i_next = n3->inputs()[0];
            if(!i_next->sources.empty())
            {
              auto cbl = i_next->sources[0];
              g->disconnect(cbl);
            }
            auto edge = ossia::make_edge(
                            ossia::immediate_strict_connection{}, n2->outputs()[0], n3->inputs()[0], n2, n3);
            g->connect(std::move(edge));
          });
        }
      }
    }
  }

  return fx.get();
}

void EffectProcessComponentBase::added(EffectComponent& e)
{

}

std::function<void ()> EffectProcessComponentBase::removing(
    const Media::Effect::EffectModel& e,
    EffectComponent& c)
{

  return {};
}

EffectProcessComponentBase::~EffectProcessComponentBase()
{
  cleanup();
}

EffectComponentFactoryList::~EffectComponentFactoryList()
{

}

EffectComponent::~EffectComponent()
{

}

EffectComponentFactory::~EffectComponentFactory()
{

}

void EffectComponentFactory::init(EffectComponent* comp) const
{

}

EffectProcessComponent::~EffectProcessComponent()
{

}


}
}
