#include "Executor.hpp"

#include <Vst/Control.hpp>
#include <Vst/Node.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/fx_node.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/network/domain/domain.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Vst::Executor)
namespace Vst
{

template <typename Node_T>
void Executor::setupNode(Node_T& node)
{
  const auto& proc = this->process();
  node->controls.reserve(proc.controls.size());
  const auto& inlets = proc.inlets();

  constexpr bool isSynth = std::remove_reference_t<decltype(*node)>::synth;
  for (std::size_t i = VST_FIRST_CONTROL_INDEX(isSynth); i < inlets.size(); i++)
  {
    auto ctrl = safe_cast<Vst::ControlInlet*>(inlets[i]);
    auto inlet = new ossia::value_inlet;

    node->controls.push_back({ctrl->fxNum, ctrl->value(), inlet->target<ossia::value_port>()});
    node->root_inputs().push_back(std::move(inlet));
  }

  std::weak_ptr<std::remove_reference_t<decltype(*node)>> wp = node;
  connect(
      &proc,
      &Vst::Model::controlAdded,
      this,
      [this, &proc, wp](const Id<Process::Port>& id) {
        auto ctrl = proc.getControl(id);
        if (!ctrl)
          return;
        if (auto n = wp.lock())
        {
          Execution::SetupContext& setup = system().context().setup;
          auto inlet = new ossia::value_inlet;

          Execution::Transaction commands{system()};

          commands.push_back([n, inlet, val = ctrl->value(), num = ctrl->fxNum] {
            n->controls.push_back({num, val, inlet->target<ossia::value_port>()});
            n->root_inputs().push_back(inlet);
          });

          setup.register_inlet(*ctrl, inlet, n, commands);

          commands.run_all();
        }
      });
  connect(
      &proc,
      &Vst::Model::controlRemoved,
      this,
      [this, wp](const Process::Port& port) {
        if (auto n = wp.lock())
        {
          Execution::SetupContext& setup = system().context().setup;
          in_exec([n, num = static_cast<const Vst::ControlInlet&>(port).fxNum] {
            auto it = ossia::find_if(n->controls, [&](auto& c) { return c.idx == num; });
            if (it != n->controls.end())
            {
              ossia::value_port* port = it->port;
              n->controls.erase(it);
              auto port_it = ossia::find_if(n->root_inputs(), [&](auto& p) {
                return p->template target<ossia::value_port>() == port;
              });
              if (port_it != n->root_inputs().end())
              {
                port->clear();
                n->root_inputs().erase(port_it);
              }
            }
          });
          setup.unregister_inlet(static_cast<const Process::Inlet&>(port), n);
        }
      });
}

Executor::Executor(
    Vst::Model& proc,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ProcessComponent_T{proc, ctx, id, "Component", parent}
{
  if (!proc.fx || !proc.fx->fx)
    throw std::runtime_error("Unable to load VST");

  AEffect& fx = *proc.fx->fx;

  if (fx.flags & effFlagsCanDoubleReplacing)
  {
    if (fx.flags & effFlagsIsSynth)
    {
      auto n = Vst::make_vst_fx<true, true>(proc.fx, ctx.execState->sampleRate);
      setupNode(n);
      node = std::move(n);
    }
    else
    {
      auto n = Vst::make_vst_fx<true, false>(proc.fx, ctx.execState->sampleRate);
      setupNode(n);
      node = std::move(n);
    }
  }
  else
  {
    if (fx.flags & effFlagsIsSynth)
    {
      auto n = Vst::make_vst_fx<false, true>(proc.fx, ctx.execState->sampleRate);
      setupNode(n);
      node = std::move(n);
    }
    else
    {
      auto n = Vst::make_vst_fx<false, false>(proc.fx, ctx.execState->sampleRate);
      setupNode(n);
      node = std::move(n);
    }
  }

  m_ossia_process = std::make_shared<ossia::node_process>(node);
}

}
