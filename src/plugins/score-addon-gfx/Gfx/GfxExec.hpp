#pragma once

#include <Process/ExecutionContext.hpp>
#include <State/ValueConversion.hpp>

#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_node.hpp>

#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxDevice.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <score_addon_gfx_export.h>

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

class SCORE_ADDON_GFX_EXPORT gfx_exec_node : public ossia::nonowning_graph_node
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
    auto inletport = new ossia::texture_inlet;
    m_inlets.push_back(inletport);
  }

  void add_audio()
  {
    auto inletport = new ossia::audio_inlet;
    m_inlets.push_back(inletport);
  }

  ~gfx_exec_node();

  int32_t id{-1};
  void run(const ossia::token_request& tk, ossia::exec_state_facade) noexcept;
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
