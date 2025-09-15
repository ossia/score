// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Component.hpp"

#include "CPUNode.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Execution/score2OSSIA.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <JS/JSProcessModel.hpp>

#if defined(SCORE_HAS_GPU_JS)
#include "GPUNode.hpp"

#include <Gfx/GfxApplicationPlugin.hpp>
#endif

#include <score/tools/Bind.hpp>

namespace JS
{
namespace Executor
{
Component::Component(
    JS::ProcessModel& element, const ::Execution::Context& ctx, QObject* parent)
    : ::Execution::ProcessComponent_T<JS::ProcessModel, ossia::node_process>{
        element, ctx, "JSComponent", parent}
{
  const bool isGpu = element.isGpu();

  if(!isGpu)
  {
    std::shared_ptr<js_node> node
        = ossia::make_node<js_node>(*ctx.execState, *ctx.execState);
    this->node = node;
    auto proc = std::make_shared<js_process>(node);
    m_ossia_process = proc;
    on_scriptChange();
  }
  else
  {
#if defined(SCORE_HAS_GPU_JS)
    std::shared_ptr<gpu_exec_node> node = ossia::make_node<gpu_exec_node>(
        *ctx.execState, ctx.doc.plugin<Gfx::DocumentPlugin>().exec);
    this->node = node;

    m_ossia_process = std::make_shared<ossia::node_process>(node);

    on_scriptChange();
#else
    throw std::runtime_error(
        "GPU Nodes not supported in this build of score, use a distribution with an "
        "up-to-date version of Qt.");
#endif
  }

  con(element, &JS::ProcessModel::programChanged, this, &Component::on_scriptChange,
      Qt::DirectConnection);
}

Component::~Component() { }

void Component::on_scriptChange()
{
  enum Type
  {
    None,
    Cpu,
    Gpu
  };
  Type cur_type{};
  Type next_type = process().isGpu() ? Gpu : Cpu;

  if(!node)
  {
    cur_type = None;
  }
  else if(auto old_node = std::dynamic_pointer_cast<js_node>(node))
  {
    cur_type = Cpu;
  }
  else
  {
    cur_type = Gpu;
  }

  Execution::Transaction commands{system()};

  // 0. Unregister all the previous inlets / outlets
  auto& setup = system().setup;
  setup.unregister_node_soft(process().inlets(), process().outlets(), node, commands);

  std::tuple<ossia::inlets, ossia::outlets, std::vector<Execution::ExecutionCommand>>
      new_ports;

  const auto& script = process().qmlData();
  // 1. Recreate ports & change the script
  if(cur_type == next_type)
  {
    if(next_type == Cpu)
      new_ports = on_cpuScriptChange(script, commands);
    else if(next_type == Gpu)
      new_ports = on_gpuScriptChange(script, commands);
  }
  else
  {
    // FIXME destroy the node
    if(next_type == Cpu)
      new_ports = on_cpuScriptChange(script, commands);
    else if(next_type == Gpu)
      new_ports = on_gpuScriptChange(script, commands);
  }

  // 3. Register the inlets / outlets
  auto& [inls, outls, controlSetups] = new_ports;

  for(std::size_t i = 0; i < inls.size(); i++)
  {
    setup.register_inlet(*process().inlets()[i], inls[i], node, commands);
  }
  for(std::size_t i = 0; i < outls.size(); i++)
  {
    setup.register_outlet(*process().outlets()[i], outls[i], node, commands);
  }

  // Additional commands once the inlets / outlets are set
  commands.commands.reserve(commands.commands.size() + controlSetups.size());
  // OPTIMIZEME std::move algorithm
  for(auto& cmd : controlSetups)
    commands.commands.push_back(std::move(cmd));
  controlSetups.clear();

  commands.run_all();

  m_oldInlets = process().inlets();
  m_oldOutlets = process().outlets();
}

std::tuple<ossia::inlets, ossia::outlets, std::vector<Execution::ExecutionCommand>>
Component::on_gpuScriptChange(const QString& script, Execution::Transaction& commands)
{
#if defined(SCORE_HAS_GPU_JS)
  auto node = std::dynamic_pointer_cast<gpu_exec_node>(this->node);
  std::weak_ptr<Gfx::gfx_exec_node> weak_node = node;
  int script_index = ++node->script_index;

  // 1. Create new inlet & outlet arrays
  ossia::inlets inls;
  ossia::outlets outls;
  Gfx::exec_controls controls;
  std::vector<Execution::ExecutionCommand> controlSetups;

  int inlet_idx = 0;
  std::size_t control_index = 0;
  {
    const Execution::Context& ctx = system();
    if(auto object = process().currentObject())
    {
      for(auto n : object->children())
      {
        auto inlet = qobject_cast<Inlet*>(n);
        auto outlet = qobject_cast<Outlet*>(n);
        if(!inlet && !outlet)
          continue;

        if([[maybe_unused]] auto val_in = qobject_cast<ValueInlet*>(inlet))
        {
          // NOTE! we do not use add_control() here as it changes the internal arrays,
          // while we will replace them after in ossia::recabler

          auto inletport = new ossia::value_inlet;
          auto control = std::make_shared<Gfx::gfx_exec_node::control>();
          control->port = &**inletport;
          control->changed = true;
          controls.push_back(control);

          inls.push_back(inletport);

          control_index++;
        }

        else if([[maybe_unused]] auto ctrl_in = qobject_cast<JS::ControlInlet*>(inlet))
        {
          auto inletport = new ossia::value_inlet;
          auto control = std::make_shared<Gfx::gfx_exec_node::control>();
          control->port = &**inletport;
          control->changed = true;

          // Configure control forwarding
          {
            auto model_port = process().inlets()[inlet_idx];
            auto model_ctrl = safe_cast<Process::ControlInlet*>(model_port);

            model_ctrl->setupExecution(*inletport, this);
            control->value = model_ctrl->value();
            control->changed = true;

            // TODO assert that we aren't going to connect twice
            QObject::disconnect(model_ctrl, nullptr, this, nullptr);
            QObject::connect(
                model_ctrl, &Process::ControlInlet::valueChanged, this,
                Gfx::con_unvalidated{ctx, control_index, script_index, weak_node});

            // FIXME initial control state
            // controlSetups.push_back([/* node, val = ctrl->value(), idx */] {
            //   // node->setControl(idx, val.apply(ossia::qt::ossia_to_qvariant{}));
            // });
          }

          controls.push_back(control);

          inls.push_back(inletport);

          control_index++;
        }
        /*
        if(auto ctrl_in = qobject_cast<ControlInlet*>(n))
        {
          inls.push_back(new ossia::value_inlet);
          inls.back()->target<ossia::value_port>()->is_event = false;

          ++idx;
        }
        else if(auto val_in = qobject_cast<ValueInlet*>(n))
        {
          inls.push_back(new ossia::value_inlet);
          auto& vp = *inls.back()->target<ossia::value_port>();

          vp.is_event = !val_in->isEvent();
          if(auto ctrl = qobject_cast<Process::ControlInlet*>(process().inlets()[idx]))
          {
            vp.type = ctrl->value().get_type();
            vp.domain = ctrl->domain().get();

            disconnect(ctrl, nullptr, this, nullptr);
            if(auto impulse = qobject_cast<Process::ImpulseButton*>(ctrl))
            {
              // connect(
              //     ctrl, &Process::ControlInlet::valueChanged, this,
              //     [this, node, idx](const ossia::value& val) {
              //   this->in_exec([node, idx] { node->impulse(idx); });
              //     });
            }
            else
            {
              // // Common case
              // connect(
              //     ctrl, &Process::ControlInlet::valueChanged, this,
              //     [this, node, idx](const ossia::value& val) {
              //   this->in_exec([node, val, idx] {
              //     node->setControl(idx, val.apply(ossia::qt::ossia_to_qvariant{}));
              //   });
              //     });
              // controlSetups.push_back([node, val = ctrl->value(), idx] {
              //   node->setControl(idx, val.apply(ossia::qt::ossia_to_qvariant{}));
              // });
            }
          }

          ++idx;
        }
        else if(auto aud_in = qobject_cast<AudioInlet*>(n))
        {
          inls.push_back(new ossia::audio_inlet);

          ++idx;
        }
        else if(auto mid_in = qobject_cast<MidiInlet*>(n))
        {
          inls.push_back(new ossia::midi_inlet);

          ++idx;
        }
        else if(auto val_out = qobject_cast<ValueOutlet*>(n))
        {
          outls.push_back(new ossia::value_outlet);
        }
        else if(auto aud_out = qobject_cast<AudioOutlet*>(n))
        {
          outls.push_back(new ossia::audio_outlet);
        }
        else if(auto mid_out = qobject_cast<MidiOutlet*>(n))
        {
          outls.push_back(new ossia::midi_outlet);
        }
*/

        else if([[maybe_unused]] auto tex_out = qobject_cast<TextureOutlet*>(outlet))
        {
          outls.push_back(new ossia::texture_outlet);
        }

        if(inlet)
          inlet_idx++;
        // else if(outlet)
        //   outlet_idx++;
      }
    }
  }

  // Send the updates to the node
  auto recable = std::shared_ptr<ossia::recabler>(
      new ossia::recabler{node, system().execGraph, inls, outls});
  commands.push_back([node, script, controls, recable]() mutable {
    using namespace std;
    // Note: we need to do this because we try to keep the Javascript node around
    // because it's slow to recreate.
    // But this causes a lot of problems, it'd be better to do like e.g. the faust
    // process and entirely recreate a new node, + call update node.
    (*recable)();

    node->setScript(std::move(script));

    swap(node->controls, controls);
  });

  SCORE_ASSERT(process().inlets().size() == inls.size());
  SCORE_ASSERT(process().outlets().size() == outls.size());
  return {std::move(inls), std::move(outls), std::move(controlSetups)};
#else
  return {};
#endif
}

std::tuple<ossia::inlets, ossia::outlets, std::vector<Execution::ExecutionCommand>>
Component::on_cpuScriptChange(const QString& script, Execution::Transaction& commands)
{
  [[maybe_unused]] auto& setup = system().setup;
  auto node = std::dynamic_pointer_cast<js_node>(this->node);

  // 1. Create new inlet & outlet arrays
  ossia::inlets inls;
  ossia::outlets outls;
  std::vector<Execution::ExecutionCommand> controlSetups;

  {
    if(auto object = process().currentObject())
    {
      int idx = 0;
      auto process_if_control
          = [this, &node, &controlSetups](ossia::value_port& vp, int idx) {
        if(auto ctrl = qobject_cast<Process::ControlInlet*>(process().inlets()[idx]))
        {
          vp.type = ctrl->value().get_type();
          vp.domain = ctrl->domain().get();

          disconnect(ctrl, nullptr, this, nullptr);
          if([[maybe_unused]] auto impulse = qobject_cast<Process::ImpulseButton*>(ctrl))
          {
            connect(
                ctrl, &Process::ControlInlet::valueChanged, this,
                [this, node, idx](const ossia::value& val) {
              in_exec([node, idx] { node->impulse(idx); });
            });
          }
          else
          {
            // Common case
            connect(
                ctrl, &Process::ControlInlet::valueChanged, this,
                [this, node, idx](const ossia::value& val) {
              in_exec([node, val, idx] {
                node->setControl(idx, val.apply(ossia::qt::ossia_to_qvariant{}));
              });
            });
            controlSetups.push_back([node, val = ctrl->value(), idx] {
              node->setControl(idx, val.apply(ossia::qt::ossia_to_qvariant{}));
            });
          }
        }
      };
      for(auto n : object->children())
      {
        if(qobject_cast<ControlInlet*>(n))
        {
          inls.push_back(new ossia::value_inlet);
          auto& vp = *inls.back()->target<ossia::value_port>();
          vp.is_event = false;

          process_if_control(vp, idx);

          ++idx;
        }
        else if(auto val_in = qobject_cast<ValueInlet*>(n))
        {
          inls.push_back(new ossia::value_inlet);
          auto& vp = *inls.back()->target<ossia::value_port>();
          vp.is_event = !val_in->isEvent();

          process_if_control(vp, idx);

          ++idx;
        }
        else if(qobject_cast<AudioInlet*>(n))
        {
          inls.push_back(new ossia::audio_inlet);

          ++idx;
        }
        else if(qobject_cast<MidiInlet*>(n))
        {
          inls.push_back(new ossia::midi_inlet);

          ++idx;
        }
        else if(qobject_cast<ValueOutlet*>(n))
        {
          outls.push_back(new ossia::value_outlet);
        }
        else if(qobject_cast<AudioOutlet*>(n))
        {
          outls.push_back(new ossia::audio_outlet);
        }
        else if(qobject_cast<MidiOutlet*>(n))
        {
          outls.push_back(new ossia::midi_outlet);
        }
      }
    }
  }

  // Send the updates to the node
  auto recable = std::shared_ptr<ossia::recabler>(
      new ossia::recabler{node, system().execGraph, inls, outls});
  commands.push_back([node, script, recable]() mutable {
    // Note: we need to do this because we try to keep the Javascript node around
    // because it's slow to recreate.
    // But this causes a lot of problems, it'd be better to do like e.g. the faust
    // process and entirely recreate a new node, + call update node.
    (*recable)();

    node->setScript(std::move(script));
  });

  SCORE_ASSERT(process().inlets().size() == inls.size());
  SCORE_ASSERT(process().outlets().size() == outls.size());
  return {std::move(inls), std::move(outls), std::move(controlSetups)};
}

}
}
