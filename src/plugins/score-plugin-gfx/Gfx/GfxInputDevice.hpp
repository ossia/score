#pragma once
#include <Gfx/GfxDevice.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Video/CameraInput.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <ossia/gfx/texture_parameter.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>


namespace Gfx
{
class video_texture_input_protocol : public ossia::net::protocol_base
{
public:
  std::shared_ptr<::Video::ExternalInput> camera;
  video_texture_input_protocol(
       std::shared_ptr<::Video::ExternalInput> cam,
      GfxExecutionAction& ctx)
      : protocol_base{flags{}}
      , camera{std::move(cam)}
      , context{&ctx}
  {
  }

  GfxExecutionAction* context{};
  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value& v) override
  {
    return false;
  }
  bool push_raw(const ossia::net::full_parameter_data&) override
  {
    return false;
  }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base& node_base) override { return false; }

  void start_execution() override { camera->start(); }
  void stop_execution() override { camera->stop(); }
};

class video_texture_input_parameter : public ossia::gfx::texture_input_parameter
{
  GfxExecutionAction* context{};

public:
  std::shared_ptr<::Video::ExternalInput> camera;
  int32_t node_id{};
  score::gfx::CameraNode* node{};

  video_texture_input_parameter(
      ossia::net::node_base& n,
      GfxExecutionAction* ctx)
      : ossia::gfx::texture_input_parameter{n}
      , context{ctx}
  {
    auto& proto = static_cast<video_texture_input_protocol&>(n.get_device().get_protocol());
    camera = proto.camera;

    node = new score::gfx::CameraNode(proto.camera, {});
    node_id = context->ui->register_node(std::unique_ptr<score::gfx::CameraNode>{node});
  }

  void pull_texture(ossia::gfx::port_index idx) override
  {
    context->setEdge(port_index{this->node_id, 0}, idx);

    score::gfx::Message m;
    m.node_id = node_id;
    context->ui->send_message(std::move(m));
  }

  virtual ~video_texture_input_parameter()
  {
    context->ui->unregister_node(node_id);
  }
};

class video_texture_input_device;
class video_texture_input_node : public ossia::net::node_base
{
  ossia::net::device_base& m_device;
  node_base* m_parent{};
  std::unique_ptr<video_texture_input_parameter> m_parameter;

public:
  video_texture_input_node(
      ossia::net::device_base& dev,
      std::string name)
      : m_device{dev}
      , m_parameter{std::make_unique<video_texture_input_parameter>(
            *this,
            dynamic_cast<video_texture_input_protocol&>(dev.get_protocol()).context)}
  {
    m_name = std::move(name);
  }

  video_texture_input_parameter* get_parameter() const override
  {
    return m_parameter.get();
  }

private:
  ossia::net::device_base& get_device() const override { return m_device; }
  ossia::net::node_base* get_parent() const override { return m_parent; }
  ossia::net::node_base& set_name(std::string) override { return *this; }
  ossia::net::parameter_base* create_parameter(ossia::val_type) override
  {
    return m_parameter.get();
  }
  bool remove_parameter() override { return false; }

  std::unique_ptr<ossia::net::node_base>
  make_child(const std::string& name) override
  {
    return {};
  }
  void removing_child(ossia::net::node_base& node_base) override { }
};

class video_texture_input_device : public ossia::net::device_base
{
  video_texture_input_node root;

public:
  video_texture_input_device(
      std::unique_ptr<ossia::net::protocol_base> proto,
      std::string name)
      : ossia::net::device_base{std::move(proto)}
      , root{*this, name}
  {
  }

  const video_texture_input_node& get_root_node() const override { return root; }
  video_texture_input_node& get_root_node() override { return root; }
};
}
