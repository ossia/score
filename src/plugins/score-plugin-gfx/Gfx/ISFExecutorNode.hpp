#pragma once
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/ISFNode.hpp>

#include <isf.hpp>
namespace Gfx
{

class filter_node final : public gfx_exec_node
{
public:
  filter_node(
      const isf::descriptor& isf, const QString& vert, const QString& frag,
      GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
    qDebug(Q_FUNC_INFO);
    auto n = std::make_unique<score::gfx::ISFNode>(isf, vert, frag);

    id = exec_context->ui->register_node(std::move(n));
  }

  void set_script(const isf::descriptor& isf, const QString& vert, const QString& frag)
  {
    exec_context->ui->unregister_node(id);

    for(int i = 0, n = std::ssize(controls); i < n; i++)
    {
      auto& ctl = controls[i];
      ctl->port->write_value(ctl->value, 0);
    }

    auto n = std::make_unique<score::gfx::ISFNode>(isf, vert, frag);

    {
      score::gfx::Message msg = exec_context->allocateMessage(this->m_inlets.size() + 1);
      msg.node_id = id;
      msg.token.date = m_last_flicks;
      msg.token.parent_duration = ossia::time_value{}; // FIXME
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
        }
        inlet_i++;
      }
      n->process(std::move(msg)); // note: node_id is incorrect at that point, it's ok
    }
    id = exec_context->ui->register_node(std::move(n));
  }

  filter_node(
      const isf::descriptor& isf, const QString& compute, GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
    qDebug(Q_FUNC_INFO);
    auto n = std::make_unique<score::gfx::ISFNode>(isf, compute);

    id = exec_context->ui->register_node(std::move(n));
  }

  void set_script(const isf::descriptor& isf, const QString& compute)
  {
    exec_context->ui->unregister_node(id);

    for(int i = 0, n = std::ssize(controls); i < n; i++)
    {
      auto& ctl = controls[i];
      ctl->port->write_value(ctl->value, 0);
    }

    auto n = std::make_unique<score::gfx::ISFNode>(isf, compute);

    {
      score::gfx::Message msg = exec_context->allocateMessage(this->m_inlets.size() + 1);
      msg.node_id = id;
      msg.token.date = m_last_flicks;
      msg.token.parent_duration = ossia::time_value{}; // FIXME
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
        }
        inlet_i++;
      }
      n->process(std::move(msg)); // note: node_id is incorrect at that point, it's ok
    }
    id = exec_context->ui->register_node(std::move(n));
  }

  ~filter_node()
  {
    qDebug(Q_FUNC_INFO);
    if(id >= 0)
      exec_context->ui->unregister_node(id);
  }

  std::string label() const noexcept override { return "Gfx::filter_node"; }
};

}
