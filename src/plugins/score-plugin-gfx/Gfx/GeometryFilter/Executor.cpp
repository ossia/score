#include "Executor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <Gfx/GeometryFilter/Process.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/GeometryFilterNode.hpp>
#include <Gfx/TexturePort.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/dataflow/graph_edge_helpers.hpp>
#include <ossia/dataflow/port.hpp>
namespace Gfx::GeometryFilter
{
class geometry_filter_node final : public gfx_exec_node
{
public:
  static inline std::atomic_int64_t filter_index = 0;
  int64_t index{};
  std::string m_shader;

  geometry_filter_node(
      const isf::descriptor& isf, const QString& vert, GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
      , index{++filter_index}
  {
    auto n = std::make_unique<score::gfx::GeometryFilterNode>(isf, update_shader(vert));

    id = exec_context->ui->register_node(std::move(n));
  }

  void set_script(const isf::descriptor& isf, const QString& vert)
  {
    exec_context->ui->unregister_node(id);

    index = ++filter_index;
    this->m_last_input_filters = {};

    for(int i = 0, n = std::ssize(controls); i < n; i++)
    {
      auto& ctl = controls[i];
      ctl->port->write_value(ctl->value, 0);
    }

    auto n = std::make_unique<score::gfx::GeometryFilterNode>(isf, update_shader(vert));
    auto msg = create_message();
    n->process(std::move(msg)); // note: node_id is incorrect at that point, it's ok

    id = exec_context->ui->register_node(std::move(n));
  }

  ~geometry_filter_node() { exec_context->ui->unregister_node(id); }

  std::string label() const noexcept override { return "Gfx::GeometryFilter_node"; }

  score::gfx::Message create_message()
  {
    score::gfx::Message msg = exec_context->allocateMessage(this->m_inlets.size() + 1);
    msg.node_id = id;
    msg.token.date = m_last_req.date;
    msg.token.parent_duration = m_last_req.parent_duration;
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
        case ossia::geometry_port::which: {
          link_cable_to_inlet(inlet, inlet_i);
          break;
        }

          // case ossia::audio_port::which: {
          //   auto& p = inlet->cast<ossia::audio_port>();
          //   msg.input[inlet_i] = std::move(p.get());
          //   break;
          // }
      }

      inlet_i++;
    }
    return msg;
  }

  QString update_shader(QString cur)
  {
    cur.replace("%node%", QString::number(index));
    m_shader = cur.toStdString();
    return cur;
  }
  void run(const ossia::token_request& tk, ossia::exec_state_facade) noexcept override
  {
    m_last_req = tk;
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

    exec_context->ui->send_message(create_message());

    SCORE_ASSERT(this->m_inlets.size() > 0);
    SCORE_ASSERT(this->m_inlets[0]->target<ossia::geometry_port>());
    SCORE_ASSERT(this->m_outlets.size() > 0);
    SCORE_ASSERT(this->m_outlets[0]->target<ossia::geometry_port>());
    auto* in_geom = this->m_inlets[0]->target<ossia::geometry_port>();
    auto* out_geom = this->m_outlets[0]->target<ossia::geometry_port>();
    out_geom->geometry.meshes = in_geom->geometry.meshes;
    out_geom->transform = in_geom->transform;
    out_geom->flags = in_geom->flags;

    // FIXME handle dirtiness
    if(m_last_input_filters == in_geom->geometry.filters)
    {
      if(m_last_index == index && out_geom->geometry.filters)
      {
        return;
      }
      else
      {
        out_geom->geometry.filters = std::make_shared<ossia::geometry_filter_list>();
        out_geom->geometry.filters->filters.push_back(
            ossia::geometry_filter{this->id, this->index, this->m_shader, m_dirty++});
      }
    }
    else
    {
      if(m_last_index == index)
        return;

      out_geom->geometry.filters = std::make_shared<ossia::geometry_filter_list>();
      if(in_geom->geometry.filters)
        for(const auto& f : in_geom->geometry.filters->filters)
          out_geom->geometry.filters->filters.push_back(f);

      out_geom->geometry.filters->filters.push_back(
          ossia::geometry_filter{this->id, this->index, this->m_shader, m_dirty++});
      m_last_input_filters = in_geom->geometry.filters;
    }

    m_last_index = index;
  }

  ossia::geometry_filter_list_ptr m_last_input_filters;
  int64_t m_dirty{-1};
  int64_t m_last_index{-1};
  ossia::token_request m_last_req{};
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::GeometryFilter::Model& element, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{element, ctx, "gfxExecutorComponent", parent}
{
  try
  {
    const auto& shader = element.processedProgram();
    const auto& desc = shader.descriptor;

    auto n = ossia::make_node<geometry_filter_node>(
        *ctx.execState, desc, shader.shader, ctx.doc.plugin<DocumentPlugin>().exec);

    for(auto* outlet : element.outlets())
    {
      if(auto out = qobject_cast<TextureOutlet*>(outlet))
      {
        out->nodeId = n->id;
      }
    }

    this->node = n;

    Execution::Transaction commands{system()};
    setup_node(commands);
    commands.run_all_in_exec();

    m_ossia_process = std::make_shared<ossia::node_process>(this->node);

    m_oldInlets = process().inlets();
    m_oldOutlets = process().outlets();

    connect(
        &element, &GeometryFilter::Model::programChanged, this,
        &ProcessExecutorComponent::on_shaderChanged, Qt::DirectConnection);
  }
  catch(...)
  {
  }
}

void ProcessExecutorComponent::cleanup()
{
  for(auto* outlet : this->process().outlets())
  {
    if(auto out = qobject_cast<TextureOutlet*>(outlet))
    {
      out->nodeId = -1;
    }
  }
}

void ProcessExecutorComponent::on_shaderChanged()
{
  auto& setup = system().setup;
  auto& element = this->process();
  Execution::Transaction commands{system()};
  auto n = std::dynamic_pointer_cast<geometry_filter_node>(this->node);

  // 0. Unregister all the previous inlets / outlets
  setup.unregister_node_soft(m_oldInlets, m_oldOutlets, node, commands);

  // 1. Recreate ports
  auto [inls, outls] = setup_node(commands);

  // 2. Change the script
  const auto& shader = element.processedProgram();
  commands.push_back(
      [n, shader = std::make_unique<GeometryFilter::ProcessedGeometryProgram>(shader)] {
    n->set_script(shader->descriptor, shader->shader);
  });

  // 3. Register the inlets / outlets
  for(std::size_t i = 0; i < inls.size(); i++)
  {
    setup.register_inlet(*process().inlets()[i], inls[i], node, commands);
  }
  for(std::size_t i = 0; i < outls.size(); i++)
  {
    setup.register_outlet(*process().outlets()[i], outls[i], node, commands);
  }

  commands.run_all();

  m_oldInlets = process().inlets();
  m_oldOutlets = process().outlets();
}

std::pair<ossia::inlets, ossia::outlets>
ProcessExecutorComponent::setup_node(Execution::Transaction& commands)
{
  const Execution::Context& ctx = system();
  auto& element = this->process();

  auto n = std::dynamic_pointer_cast<geometry_filter_node>(this->node);
  int script_index = ++n->script_index;

  // 1. Create new inlet & outlet arrays
  ossia::inlets inls;
  ossia::outlets outls;

  std::vector<std::shared_ptr<gfx_exec_node::control>> controls;
  std::vector<Execution::ExecutionCommand> controlSetups;

  std::size_t control_index = 0;
  std::weak_ptr<gfx_exec_node> weak_node = n;
  for(auto& ctl : element.inlets())
  {
    if(auto ctrl = qobject_cast<Process::ControlInlet*>(ctl))
    {
      // NOTE! we do not use add_control() here as it changes the internal arrays,
      // while we will replace them after in ossia::recabler

      auto inletport = new ossia::value_inlet;
      auto control = std::make_shared<gfx_exec_node::control>();
      control->port = &**inletport;
      control->changed = true;
      control->value = ctrl->value();
      controls.push_back(control);

      inls.push_back(inletport);
      ctl->setupExecution(*inletport, this);

      // TODO assert that we aren't going to connect twice
      QObject::disconnect(ctrl, nullptr, this, nullptr);
      QObject::connect(
          ctrl, &Process::ControlInlet::valueChanged, this,
          con_unvalidated{ctx, control_index, script_index, weak_node});

      control_index++;
    }
    else if([[maybe_unused]] auto ctrl = qobject_cast<Process::AudioInlet*>(ctl))
    {
      inls.push_back(new ossia::audio_inlet);
    }
    else if(auto ctrl = qobject_cast<Gfx::TextureInlet*>(ctl))
    {
      auto tex = new ossia::texture_inlet;
      inls.push_back(tex);
      ctrl->setupExecution(*tex, this);
    }
    else if([[maybe_unused]] auto ctrl = qobject_cast<Gfx::GeometryInlet*>(ctl))
    {
      inls.push_back(new ossia::geometry_inlet);
    }
  }

  outls.push_back(new ossia::geometry_outlet);

  //! TODO the day we have audio outputs in some GFX node
  //! propagate will need to be handled ; right now here
  //! it will cut the sound
  auto recable = std::shared_ptr<ossia::recabler>(
      new ossia::recabler{n, system().execGraph, inls, outls});
  commands.push_back([n, controls, recable]() mutable {
    using namespace std;
    (*recable)();

    swap(n->controls, controls);
  });

  return {std::move(inls), std::move(outls)};
}
}
