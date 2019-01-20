#include "VSTExecutor.hpp"

#include <Media/Effect/VST/VSTControl.hpp>
#include <Media/Effect/VST/VSTNode.hpp>
#include <Process/ExecutionContext.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/network/domain/domain.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Execution::VSTEffectComponent)
namespace Execution
{
VSTEffectComponent::VSTEffectComponent(
    Media::VST::VSTEffectModel& proc, const Execution::Context& ctx,
    const Id<score::Component>& id, QObject* parent)
    : ProcessComponent_T{proc, ctx, id, "VSTComponent", parent}
{
  if (!proc.fx || !proc.fx->fx)
    throw std::runtime_error("Unable to load VST");

  AEffect& fx = *proc.fx->fx;
  auto setup_controls = [&](auto& node) {
    node->controls.reserve(proc.controls.size());
    const auto& inlets = proc.inlets();
    for (std::size_t i = 3; i < inlets.size(); i++)
    {
      auto ctrl = safe_cast<Media::VST::VSTControlInlet*>(inlets[i]);
      auto inlet = ossia::make_inlet<ossia::value_port>();

      node->controls.push_back(
          {ctrl->fxNum, ctrl->value(), inlet->data.target<ossia::value_port>()});
      node->inputs().push_back(std::move(inlet));
    }

    std::weak_ptr<std::remove_reference_t<decltype(*node)>> wp = node;
    connect(
        &proc, &Media::VST::VSTEffectModel::controlAdded, this,
        [this, &proc, wp](const Id<Process::Port>& id) {
          auto ctrl = proc.getControl(id);
          if (!ctrl)
            return;
          if (auto n = wp.lock())
          {
            in_exec([n, val = ctrl->value(), num = ctrl->fxNum] {
              auto inlet = ossia::make_inlet<ossia::value_port>();

              n->controls.push_back(
                  {num, val, inlet->data.target<ossia::value_port>()});
              n->inputs().push_back(inlet);
            });
          }
        });
    connect(
        &proc, &Media::VST::VSTEffectModel::controlRemoved, this,
        [this, wp](const Process::Port& port) {
          if (auto n = wp.lock())
          {
            in_exec(
                [n, num = static_cast<const Media::VST::VSTControlInlet&>(port)
                              .fxNum] {
                  auto it = ossia::find_if(
                      n->controls, [&](auto& c) { return c.idx == num; });
                  if (it != n->controls.end())
                  {
                    auto port = it->port;
                    n->controls.erase(it);
                    auto port_it = ossia::find_if(n->inputs(), [&](auto& p) {
                      return p->data.target() == port;
                    });
                    if (port_it != n->inputs().end())
                    {
                      port->clear();
                      n->inputs().erase(port_it);
                    }
                  }
                });
          }
        });

    using ptr_t = std::remove_reference_t<decltype(node)>;
    typename ptr_t::weak_type weak_node = node;
    con(ctx.doc.coarseUpdateTimer, &QTimer::timeout, this, [weak_node, &proc] {
      if(auto node = weak_node.lock())
      {
        for (std::size_t i = 3; i < proc.inlets().size(); i++)
        {
          auto inlet = static_cast<Media::VST::VSTControlInlet*>(proc.inlets()[i]);
          inlet->setValue(node->controls[i - 3].value);
        }
      }
    });
  };

  if (fx.flags & effFlagsCanDoubleReplacing)
  {
    if (fx.flags & effFlagsIsSynth)
    {
      auto n = Media::VST::make_vst_fx<true, true>(
          proc.fx, ctx.execState->sampleRate);
      setup_controls(n);
      node = std::move(n);
    }
    else
    {
      auto n = Media::VST::make_vst_fx<true, false>(
          proc.fx, ctx.execState->sampleRate);
      setup_controls(n);
      node = std::move(n);
    }
  }
  else
  {
    if (fx.flags & effFlagsIsSynth)
    {
      auto n = Media::VST::make_vst_fx<false, true>(
          proc.fx, ctx.execState->sampleRate);
      setup_controls(n);
      node = std::move(n);
    }
    else
    {
      auto n = Media::VST::make_vst_fx<false, false>(
          proc.fx, ctx.execState->sampleRate);
      setup_controls(n);
      node = std::move(n);
    }
  }

  m_ossia_process = std::make_shared<ossia::node_process>(node);
}
}
