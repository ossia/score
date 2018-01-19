#include "VSTExecutor.hpp"
#include <Media/Effect/VST/VSTControl.hpp>
#include <Media/Effect/VST/VSTNode.hpp>

#include <ossia/detail/logger.hpp>
#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/dataflow/execution_state.hpp>

namespace Engine::Execution
{
VSTEffectComponent::VSTEffectComponent(
    Media::VST::VSTEffectModel& proc,
    const Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ProcessComponent_T{proc, ctx, id, "VSTComponent", parent}
{
  AEffect& fx = *proc.fx->fx;
  auto setup_controls = [&] (auto& node) {
    node->ctrl_ptrs.reserve(proc.controls.size());
    const auto& inlets = proc.inlets();
    for(std::size_t i = 3; i < inlets.size(); i++)
    {
      auto ctrl = safe_cast<Media::VST::VSTControlInlet*>(inlets[i]);
      auto inlet = ossia::make_inlet<ossia::value_port>();

      node->ctrl_ptrs.push_back({ctrl->fxNum, inlet->data.target<ossia::value_port>()});
      node->inputs().push_back(std::move(inlet));
    }

    std::weak_ptr<std::remove_reference_t<decltype(*node)>> wp = node;
    connect(&proc, &Media::VST::VSTEffectModel::controlAdded,
            this, [this,&proc,wp] (const Id<Process::Port>& id) {
      auto port = proc.getControl(id);
      if(!port)
        return;
      if(auto n = wp.lock())
      {
        in_exec([n,num=port->fxNum] {
          auto inlet = ossia::make_inlet<ossia::value_port>();

          n->ctrl_ptrs.push_back({num, inlet->data.target<ossia::value_port>()});
          n->inputs().push_back(inlet);
        });
      }
    });
    connect(&proc, &Media::VST::VSTEffectModel::controlRemoved,
            this, [this,wp] (const Process::Port& port) {
      if(auto n = wp.lock())
      {
        in_exec([n,num=static_cast<const Media::VST::VSTControlInlet&>(port).fxNum] {
          auto it = ossia::find_if(n->ctrl_ptrs, [&] (auto& c) {
            return c.first == num;
          });
          if(it != n->ctrl_ptrs.end())
          {
            auto port = it->second;
            n->ctrl_ptrs.erase(it);
            auto port_it = ossia::find_if(n->inputs(), [&] (auto& p) {
              return p->data.target() == port;
            });
            if(port_it != n->inputs().end())
            {
              port->clear();
              n->inputs().erase(port_it);
            }
          }
        });
      }
    });
  };

  if(fx.flags & effFlagsCanDoubleReplacing)
  {
    if(fx.flags & effFlagsIsSynth)
    {
      auto n = Media::VST::make_vst_fx<true, true>(proc.fx, ctx.plugin.execState->sampleRate);
      setup_controls(n);
      node = std::move(n);
    }
    else
    {
      auto n = Media::VST::make_vst_fx<true, false>(proc.fx, ctx.plugin.execState->sampleRate);
      setup_controls(n);
      node = std::move(n);
    }
  }
  else
  {
    if(fx.flags & effFlagsIsSynth)
    {
      auto n = Media::VST::make_vst_fx<false, true>(proc.fx, ctx.plugin.execState->sampleRate);
      setup_controls(n);
      node = std::move(n);
    }
    else
    {
      auto n = Media::VST::make_vst_fx<false, false>(proc.fx, ctx.plugin.execState->sampleRate);
      setup_controls(n);
      node = std::move(n);
    }
  }

  m_ossia_process = std::make_shared<ossia::node_process>(node);
}
}
