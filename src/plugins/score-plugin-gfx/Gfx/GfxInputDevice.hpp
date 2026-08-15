#pragma once
#include <Gfx/GfxDevice.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <ossia/gfx/texture_parameter.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
namespace Video
{
class ExternalInput;
}
namespace score::gfx {
class CameraNode;
}
namespace Gfx
{

class simple_texture_input_protocol : public ossia::net::protocol_base
{
public:
  simple_texture_input_protocol()
      : protocol_base{flags{}}
  {
  }

  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value& v) override
  {
    return false;
  }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base& node_base) override { return false; }

  void start_execution() override { }
  void stop_execution() override { }
};

class simple_texture_input_parameter : public ossia::gfx::texture_parameter
{
  GfxExecutionAction* context{};

public:
  int32_t node_id{score::gfx::invalid_node_index};
  score::gfx::Node* node{};

  simple_texture_input_parameter(
      score::gfx::Node* gfx_n, GfxExecutionAction* ctx, ossia::net::node_base& n)
      : ossia::gfx::texture_parameter{n}
      , context{ctx}
      , node{gfx_n}
  {
    node_id = context->ui->register_node(std::unique_ptr<score::gfx::Node>{gfx_n});
  }

  void pull_texture(ossia::gfx::port_index idx) override
  {
    context->setEdge(
        port_index{this->node_id, 0}, idx, Process::CableType::ImmediateGlutton);

    score::gfx::Message m;
    m.node_id = node_id;
    context->ui->send_message(std::move(m));
  }

  virtual ~simple_texture_input_parameter() { context->ui->unregister_node(node_id); }
};

class simple_texture_input_device;
class simple_texture_input_node : public ossia::net::node_base
{
  ossia::net::device_base& m_device;
  node_base* m_parent{};
  std::unique_ptr<simple_texture_input_parameter> m_parameter;

public:
  simple_texture_input_node(
      score::gfx::Node* gfx_n, GfxExecutionAction* context, ossia::net::device_base& dev,
      std::string name)
      : m_device{dev}
      , m_parameter{
            std::make_unique<simple_texture_input_parameter>(gfx_n, context, *this)}
  {
    m_name = std::move(name);
  }

  simple_texture_input_parameter* get_parameter() const override
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

  std::unique_ptr<ossia::net::node_base> make_child(const std::string& name) override
  {
    return {};
  }
  void removing_child(ossia::net::node_base& node_base) override { }
};

class simple_texture_input_device : public ossia::net::device_base
{
  simple_texture_input_node root;

public:
  simple_texture_input_device(
      score::gfx::Node* gfx_n, GfxExecutionAction* context,
      std::unique_ptr<ossia::net::protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}
      , root{gfx_n, context, *this, name}
  {
  }

  const simple_texture_input_node& get_root_node() const override { return root; }
  simple_texture_input_node& get_root_node() override { return root; }
};

/**
 * @brief One stream of a device that exposes several.
 *
 * Same parameter as the single-stream node -- a texture bound to its own
 * score::gfx::Node -- but living as a child, so a rig of N sensors is one
 * device addressed `rig:/cam0`, `rig:/cam1` rather than N devices that have to
 * be configured consistently by hand.
 */
class texture_input_stream_node final : public ossia::net::node_base
{
  ossia::net::device_base& m_device;
  ossia::net::node_base* m_parent{};
  std::unique_ptr<simple_texture_input_parameter> m_parameter;

public:
  texture_input_stream_node(
      score::gfx::Node* gfx_n, GfxExecutionAction* context,
      ossia::net::device_base& dev, ossia::net::node_base& parent, std::string name)
      : m_device{dev}
      , m_parent{&parent}
      , m_parameter{
            std::make_unique<simple_texture_input_parameter>(gfx_n, context, *this)}
  {
    m_name = std::move(name);
  }

  simple_texture_input_parameter* get_parameter() const override
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
  std::unique_ptr<ossia::net::node_base> make_child(const std::string&) override
  {
    return {};
  }
  void removing_child(ossia::net::node_base&) override { }
};

/**
 * @brief Root of a device exposing several texture streams.
 *
 * Carries no parameter of its own: the root is the rig, and the frames belong
 * to its children. Streams are added up front by the device rather than
 * created on demand, because each one has to be handed the gfx node it renders.
 */
class multi_texture_input_node final : public ossia::net::node_base
{
  ossia::net::device_base& m_device;

public:
  multi_texture_input_node(ossia::net::device_base& dev, std::string name)
      : m_device{dev}
  {
    m_name = std::move(name);
  }

  ossia::net::node_base*
  add_stream(score::gfx::Node* gfx_n, GfxExecutionAction* context, std::string name)
  {
    return add_child(std::make_unique<texture_input_stream_node>(
        gfx_n, context, m_device, *this, std::move(name)));
  }

  ossia::net::parameter_base* get_parameter() const override { return nullptr; }

private:
  ossia::net::device_base& get_device() const override { return m_device; }
  ossia::net::node_base* get_parent() const override { return nullptr; }
  ossia::net::node_base& set_name(std::string) override { return *this; }
  ossia::net::parameter_base* create_parameter(ossia::val_type) override
  {
    return nullptr;
  }
  bool remove_parameter() override { return false; }
  std::unique_ptr<ossia::net::node_base> make_child(const std::string&) override
  {
    return {};
  }
  void removing_child(ossia::net::node_base&) override { }
};

class multi_texture_input_device final : public ossia::net::device_base
{
  multi_texture_input_node root;

public:
  multi_texture_input_device(
      std::unique_ptr<ossia::net::protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}
      , root{*this, std::move(name)}
  {
  }

  ossia::net::node_base*
  add_stream(score::gfx::Node* gfx_n, GfxExecutionAction* context, std::string name)
  {
    return root.add_stream(gfx_n, context, std::move(name));
  }

  const multi_texture_input_node& get_root_node() const override { return root; }
  multi_texture_input_node& get_root_node() override { return root; }
};

class SCORE_PLUGIN_GFX_EXPORT video_texture_input_protocol
    : public ossia::net::protocol_base
{
public:
  std::shared_ptr<::Video::ExternalInput> camera;
  video_texture_input_protocol(
      std::shared_ptr<::Video::ExternalInput> cam, GfxExecutionAction& ctx);
  ~video_texture_input_protocol();

  GfxExecutionAction* context{};
  bool pull(ossia::net::parameter_base&) override;
  bool push(const ossia::net::parameter_base&, const ossia::value& v) override;
  bool push_raw(const ossia::net::full_parameter_data&) override;
  bool observe(ossia::net::parameter_base&, bool) override;
  bool update(ossia::net::node_base& node_base) override;

  void start_execution() override;
  void stop_execution() override;

  score::gfx::CameraNode* camera_node{};
};

class SCORE_PLUGIN_GFX_EXPORT video_texture_input_parameter
    : public ossia::gfx::texture_parameter
{
  video_texture_input_protocol& proto;
  GfxExecutionAction* context{};

public:
  std::shared_ptr<::Video::ExternalInput> camera;
  int32_t node_id{};
  score::gfx::CameraNode* node{};

  video_texture_input_parameter(
      ossia::net::node_base& n, video_texture_input_protocol& proto);
  void pull_texture(ossia::gfx::port_index idx) override;

  ~video_texture_input_parameter();
};

class video_texture_input_device;
class SCORE_PLUGIN_GFX_EXPORT video_texture_input_node : public ossia::net::node_base
{
  ossia::net::device_base& m_device;
  node_base* m_parent{};
  std::unique_ptr<video_texture_input_parameter> m_parameter;

public:
  video_texture_input_node(ossia::net::device_base& dev, std::string name);
  ~video_texture_input_node();

  video_texture_input_parameter* get_parameter() const override;

private:
  ossia::net::device_base& get_device() const override;
  ossia::net::node_base* get_parent() const override;
  ossia::net::node_base& set_name(std::string) override;
  ossia::net::parameter_base* create_parameter(ossia::val_type) override;
  bool remove_parameter() override;

  std::unique_ptr<ossia::net::node_base> make_child(const std::string& name) override;
  void removing_child(ossia::net::node_base& node_base) override;
};

class SCORE_PLUGIN_GFX_EXPORT video_texture_input_device : public ossia::net::device_base
{
  video_texture_input_node root;

public:
  video_texture_input_device(
      std::unique_ptr<ossia::net::protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}
      , root{*this, name}
  {
  }
  ~video_texture_input_device();

  const video_texture_input_node& get_root_node() const override { return root; }
  video_texture_input_node& get_root_node() override { return root; }
};

}
