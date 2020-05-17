#pragma once

#include <Process/ExecutionContext.hpp>
#include <State/ValueConversion.hpp>

#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_node.hpp>

#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxDevice.hpp>
#include <Gfx/GfxExecContext.hpp>
namespace Gfx
{

template <typename Vector>
int64_t index_of(Vector&& v, const typename std::remove_reference_t<Vector>::value_type& t)
{
  if (auto it = ossia::find(v, t); it != v.end())
  {
    return std::distance(v.begin(), it);
  }
  return -1;
}

class gfx_exec_node : public ossia::nonowning_graph_node
{
public:
  struct control
  {
    ossia::value* value{};
    ossia::value_port* port{};
    bool changed{};
  };
  std::vector<control> controls;
  GfxExecutionAction* exec_context{};
  gfx_exec_node(GfxExecutionAction& e_ctx) : exec_context{&e_ctx} { }

  control& add_control()
  {
    auto inletport = new ossia::value_inlet;
    controls.push_back(control{new ossia::value, &**inletport, false});
    m_inlets.push_back(inletport);
    return controls.back();
  }

  void add_texture()
  {
    auto inletport = new ossia::value_inlet;
    m_inlets.push_back(inletport);
  }

  void add_audio()
  {
    auto inletport = new ossia::audio_inlet;
    m_inlets.push_back(inletport);
  }

  ~gfx_exec_node()
  {
    for (auto ctl : controls)
      delete ctl.value;
  }

  int32_t id{-1};
  void run(const ossia::token_request& tk, ossia::exec_state_facade) noexcept override
  {
    {
      // Copy all the UI controls
      const int n = controls.size();
      for (int i = 0; i < n; i++)
      {
        auto& ctl = controls[i];
        if (ctl.changed)
        {
          ctl.port->write_value(std::move(*ctl.value), 0);
          ctl.changed = false;
        }
      }
    }
    gfx_message msg;
    msg.node_id = id;
    msg.token = tk;

    msg.inputs.resize(this->m_inlets.size());
    int inlet_i = 0;
    for (ossia::inlet* inlet : this->m_inlets)
    {
      for (ossia::graph_edge* cable : inlet->sources)
      {
        if (auto src_gfx = dynamic_cast<gfx_exec_node*>(cable->out_node.get()))
        {
          if (src_gfx->executed())
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

      switch (inlet->which())
      {
        case ossia::value_port::which:
        {
          auto& p = inlet->cast<ossia::value_port>();
          for (ossia::timed_value& val : p.get_data())
          {
            msg.inputs[inlet_i].push_back(std::move(val.value));
          }
          break;
        }
        case ossia::audio_port::which:
        {
          auto& p = inlet->cast<ossia::audio_port>();
          msg.inputs[inlet_i].push_back(std::move(p.samples));
          break;
        }
      }

      inlet_i++;
    }

    auto out = this->m_outlets[0]->address.target<ossia::net::parameter_base*>();
    if (out)
    {
      if (auto p = dynamic_cast<gfx_parameter*>(*out))
      {
        p->push_texture({this->id, 0});
      }
    }

    exec_context->ui->tick_messages.enqueue(std::move(msg));
  }
};

struct control_updater
{
  gfx_exec_node::control& ctrl;
  ossia::value v;

  void operator()() noexcept
  {
    *ctrl.value = std::move(v);
    ctrl.changed = true;
  }
};

struct con_unvalidated
{
  const Execution::Context& ctx;
  const int i;
  std::weak_ptr<gfx_exec_node> weak_node;
  void operator()(const ossia::value& val)
  {
    if (auto node = weak_node.lock())
    {
      ctx.executionQueue.enqueue(control_updater{node->controls[i], val});
    }
  }
};

}
