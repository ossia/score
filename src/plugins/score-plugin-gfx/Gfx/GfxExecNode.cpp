#include <Gfx/CameraDevice.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/GfxParameter.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <ossia/dataflow/port.hpp>
#include <ossia/detail/ssize.hpp>
#include <ossia/network/value/format_value.hpp>
namespace Gfx
{
gfx_exec_node::~gfx_exec_node()
{
  controls.clear();
}

void gfx_exec_node::link_cable_to_inlet(ossia::inlet* inlet, int inlet_i)
{
  for(ossia::graph_edge* cable : inlet->sources)
  {
    // Should be impossible to not be connected to a gfx_exec_node
    if(auto src_gfx = dynamic_cast<gfx_exec_node*>(cable->out_node.get()))
    {
      if(src_gfx->executed())
      {
        int32_t port_idx = index_of(src_gfx->m_outlets, cable->out);
        assert(port_idx != -1);
        {
          exec_context->setEdge(
              port_index{src_gfx->id, port_idx}, port_index{this->id, inlet_i});
        }
      }
    }
  }
}

void gfx_exec_node::run(
    const ossia::token_request& tk, ossia::exec_state_facade) noexcept
{
  m_last_flicks = tk.date;
  {
    // Copy all the UI controls
    const int n = std::ssize(controls);
    for(int i = 0; i < n; i++)
    {
      auto& ctl = controls[i];
      if(ctl->changed)
      {
        ctl->port->write_value(ctl->value, 0);
        ctl->changed = false;
      }
    }
  }

  score::gfx::Message msg = exec_context->allocateMessage(this->m_inlets.size() + 1);
  msg.node_id = id;
  msg.token.date = tk.date;
  msg.token.parent_duration = tk.parent_duration;
  msg.input.resize(this->m_inlets.size());
  int inlet_i = 0;
  for(ossia::inlet* inlet : this->m_inlets)
  {
    switch(inlet->which())
    {
      case ossia::value_port::which: {
        auto& p = inlet->cast<ossia::value_port>();
        if(!p.get_data().empty())
        {
          msg.input[inlet_i] = std::move(p.get_data().back().value);

          p.get_data().clear();
        }

        break;
      }

      case ossia::texture_port::which: {
        if(auto in = inlet->address.target<ossia::net::parameter_base*>();
           in && (*in)->get_type() == ossia::parameter_type::TEXTURE)
        {
          auto cam = static_cast<ossia::gfx::texture_parameter*>(*in);
          cam->pull_texture({this->id, inlet_i});
        }

        auto& p = inlet->cast<ossia::texture_inlet>();
        msg.input[inlet_i] = p.data;
        link_cable_to_inlet(inlet, inlet_i);
        break;
      }

      case ossia::geometry_port::which: {
        auto& p = inlet->cast<ossia::geometry_port>();
        p.flags = {}; // FIXME unneeded now?

        link_cable_to_inlet(inlet, inlet_i);
        break;
      }

      case ossia::audio_port::which: {
        auto& p = inlet->cast<ossia::audio_port>();
        msg.input[inlet_i] = std::move(p.get());
        break;
      }
    }

    inlet_i++;
  }

  for(auto& outlet : this->m_outlets)
  {
    if(auto out = outlet->address.target<ossia::net::parameter_base*>())
    {
      // TODO same, ugh.
      if(auto p = dynamic_cast<gfx_parameter_base*>(*out))
      {
        p->push_texture({this->id, 0});
      }
    }
  }

  exec_context->ui->send_message(std::move(msg));

  // Finally: if there are any new output controls, handle them
  for(auto& out_ctl : control_outs)
  {
    if(std::exchange(out_ctl->changed, false))
      out_ctl->port->write_value(std::move(out_ctl->value), 0);
  }
}

void con_unvalidated::operator()(const ossia::value& val)
{
  struct control_updater
  {
    std::shared_ptr<gfx_exec_node::control> ctrl;
    ossia::value v;

    void operator()() noexcept
    {
      ctrl->value = std::move(v);
      ctrl->changed = true;
    }
  };

  if(auto node = weak_node.lock())
  {
    // Check for the case where the node controls have changed
    // due to the script changing
    if(script_index != node->script_index)
      return;

    // This can happen if we sent controls from the UI before the execution engine had the time to add them
    // Note: ideally something should be fixed to make that fit with the case above
    if(i >= node->controls.size())
      return;
    ctx.executionQueue.enqueue(control_updater{node->controls[i], val});
  }
}
}
