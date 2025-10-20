#pragma once

#include <State/ValueConversion.hpp>

#include <Process/ExecutionContext.hpp>

#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxDevice.hpp>
#include <Gfx/GfxExecContext.hpp>

#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

#include <score_plugin_gfx_export.h>

namespace Gfx
{

template <typename Vector>
int64_t
index_of(Vector&& v, const typename std::remove_reference_t<Vector>::value_type& t)
{
  if(auto it = ossia::find(v, t); it != v.end())
  {
    return std::distance(v.begin(), it);
  }
  return -1;
}

struct exec_control
{
  ossia::value value{};
  ossia::value_port* port{};
  bool changed{};
};

using exec_controls = std::vector<std::shared_ptr<exec_control>>;
class SCORE_PLUGIN_GFX_EXPORT gfx_exec_node : public ossia::graph_node
{
public:
  using control = Gfx::exec_control;

  exec_controls controls;
  exec_controls control_outs;

  GfxExecutionAction* exec_context{};
  gfx_exec_node(GfxExecutionAction& e_ctx)
      : exec_context{&e_ctx}
  {
  }

  const std::shared_ptr<control>& add_control()
  {
    auto port = new ossia::value_inlet;
    m_inlets.push_back(port);

    controls.push_back(std::make_shared<control>());
    auto& c = controls.back();
    c->port = &**port;
    c->changed = true;

    return c;
  }

  const std::shared_ptr<control>& add_control_out()
  {
    auto port = new ossia::value_outlet;
    m_outlets.push_back(port);

    control_outs.push_back(std::make_shared<control>());
    auto& c = control_outs.back();
    c->port = &**port;
    c->changed = false;

    return c;
  }

  auto add_value_port()
  {
    auto port = new ossia::value_inlet;
    m_inlets.push_back(port);
    return port;
  }

  auto add_texture()
  {
    auto port = new ossia::texture_inlet;
    m_inlets.push_back(port);
    return port;
  }

  auto add_texture_out()
  {
    auto port = new ossia::texture_outlet;
    m_outlets.push_back(port);
    return port;
  }

  auto add_geometry()
  {
    auto port = new ossia::geometry_inlet;
    m_inlets.push_back(port);
    return port;
  }

  auto add_geometry_out()
  {
    auto port = new ossia::geometry_outlet;
    m_outlets.push_back(port);
    return port;
  }

  auto add_audio()
  {
    auto port = new ossia::audio_inlet;
    m_inlets.push_back(port);
    return port;
  }

  ~gfx_exec_node();

  int32_t id{-1};
  std::atomic_int32_t script_index{0};
  ossia::time_value m_last_flicks{};
  void run(const ossia::token_request& tk, ossia::exec_state_facade) noexcept override;

  void link_cable_to_inlet(ossia::inlet* inlet, int inlet_i);
  using ossia::graph_node::m_inlets;
  using ossia::graph_node::m_outlets;
};

struct SCORE_PLUGIN_GFX_EXPORT con_unvalidated
{
  const Execution::Context& ctx;
  const std::size_t i;
  const int32_t script_index{};
  std::weak_ptr<gfx_exec_node> weak_node;
  void operator()(const ossia::value& val);
};

}
