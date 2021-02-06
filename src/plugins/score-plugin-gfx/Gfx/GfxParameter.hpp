#pragma once
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>

#include <ossia/gfx/texture_parameter.hpp>


#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecContext.hpp>

namespace Gfx
{
class gfx_parameter_base : public ossia::gfx::texture_parameter
{
protected:
  GfxExecutionAction* context{};

public:
  NodeModel* node{};
  int32_t node_id{};

  gfx_parameter_base(ossia::net::node_base& n, NodeModel* node, GfxExecutionAction* ctx)
    : texture_parameter{n}, context{ctx}, node{node}
  {
    node_id = context->ui->register_node(std::unique_ptr<NodeModel>{node});
  }

  void push_texture(port_index idx) { context->setEdge(idx, port_index{this->node_id, 0}); }

  virtual ~gfx_parameter_base() { context->ui->unregister_node(node_id); }
};

class gfx_protocol_base : public ossia::net::protocol_base
{
public:
  gfx_protocol_base(GfxExecutionAction& ctx)
      : protocol_base{flags{}}
      , context{&ctx} { }
  GfxExecutionAction* context{};
  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value& v) override { return false; }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base& node_base) override { return false; }
};

class gfx_node_base : public ossia::net::node_base
{
  ossia::net::device_base& m_device;
  gfx_node_base* m_parent{};
  std::unique_ptr<gfx_parameter_base> m_parameter;

public:
  gfx_node_base(ossia::net::device_base& dev, NodeModel* gfxmodel, std::string name)
      : m_device{dev}
      , m_parameter{std::make_unique<gfx_parameter_base>(
            *this,
            gfxmodel,
            dynamic_cast<gfx_protocol_base&>(dev.get_protocol()).context)}
  {
    m_name = std::move(name);
  }

  gfx_parameter_base* get_parameter() const override { return m_parameter.get(); }

private:
  ossia::net::device_base& get_device() const override { return m_device; }
  ossia::net::node_base* get_parent() const override { return m_parent; }
  ossia::net::node_base& set_name(std::string) override { return *this; }
  ossia::net::parameter_base* create_parameter(ossia::val_type) override
  {
    return m_parameter.get();
  }
  bool remove_parameter() override { return false; }

  std::unique_ptr<ossia::net::node_base> make_child(const std::string& name) override
  {
    return {};
  }
  void removing_child(ossia::net::node_base& node_base) override { }
};

}


