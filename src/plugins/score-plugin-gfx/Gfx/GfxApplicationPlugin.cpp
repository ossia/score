#include "GfxApplicationPlugin.hpp"

#include <score/tools/IdentifierGeneration.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <ossia/dataflow/port.hpp>

#include <Execution/DocumentPlugin.hpp>

#include <Gfx/GfxParameter.hpp>
#include <Gfx/CameraDevice.hpp>
#include "GfxExec.hpp"
namespace Gfx
{

gfx_exec_node::~gfx_exec_node()
{
  for (auto ctl : controls)
    delete ctl.value;
}


void gfx_exec_node::run(const ossia::token_request& tk, ossia::exec_state_facade) noexcept
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
    case ossia::texture_port::which:
    {
      if (auto& addrs = inlet->addresses; !addrs.empty())
      if (auto in = addrs[0].target<ossia::net::parameter_base*>())
      {
        // TODO remove this dynamic_cast. maybe target should have
        // audio_parameter / texture_parameter / midi_parameter ... cases
        // does not scale though
        if(auto cam = dynamic_cast<ossia::gfx::texture_input_parameter*>(*in))
        {
          cam->pull_texture({this->id, inlet_i});
        }
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

  if (auto& addrs = this->m_outlets[0]->addresses; !addrs.empty())
  if (auto out = addrs[0].target<ossia::net::parameter_base*>())
  {
    if (auto p = dynamic_cast<gfx_parameter_base*>(*out))
    {
      p->push_texture({this->id, 0});
    }
  }

  exec_context->ui->tick_messages.enqueue(std::move(msg));
}


DocumentPlugin::DocumentPlugin(
    const score::DocumentContext& ctx,
    Id<score::DocumentPlugin> id,
    QObject* parent)
    : score::DocumentPlugin{ctx, std::move(id), "Gfx::DocumentPlugin", parent}
{
  auto& exec_plug = ctx.plugin<Execution::DocumentPlugin>();
  exec_plug.registerAction(exec);
}

DocumentPlugin::~DocumentPlugin() { }

ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& app)
    : GUIApplicationPlugin{app}
{
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  doc.model().addPluginModel(
      new DocumentPlugin{doc.context(), getStrongId(doc.model().pluginModels()), &doc.model()});
}

}
