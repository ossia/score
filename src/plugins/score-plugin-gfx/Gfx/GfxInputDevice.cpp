#include "GfxInputDevice.hpp"

namespace Gfx
{

void video_texture_input_protocol::start_execution()
{
  camera->start();
}

void video_texture_input_protocol::stop_execution()
{
  if(camera_node)
  {
    // avformat_close_input *must* be called after all the frames have
    // been freed ; since rendering may be using an AVFrame in another thread
    // we may have to wait until rendering has entirely stopped
    // which is checked in CameraNode::renderedNodesChanged()
    if(camera_node->renderedNodes.empty())
    {
      camera_node->reader.releaseAllFrames();
      camera->stop();
    }
    else
    {
      camera_node->must_stop = true;
    }
  }
}

video_texture_input_device::~video_texture_input_device() { }

video_texture_input_protocol::video_texture_input_protocol(
    std::shared_ptr<::Video::ExternalInput> cam, GfxExecutionAction& ctx)
    : protocol_base{flags{}}
    , camera{std::move(cam)}
    , context{&ctx}
{
}

video_texture_input_protocol::~video_texture_input_protocol() { }

bool video_texture_input_protocol::pull(ossia::net::parameter_base&)
{
  return false;
}

bool video_texture_input_protocol::push(
    const ossia::net::parameter_base&, const ossia::value& v)
{
  return false;
}

bool video_texture_input_protocol::push_raw(const ossia::net::full_parameter_data&)
{
  return false;
}

bool video_texture_input_protocol::observe(ossia::net::parameter_base&, bool)
{
  return false;
}

bool video_texture_input_protocol::update(ossia::net::node_base& node_base)
{
  return false;
}

video_texture_input_parameter::video_texture_input_parameter(
    ossia::net::node_base& n, video_texture_input_protocol& proto)
    : ossia::gfx::texture_input_parameter{n}
    , proto{proto}
    , context{proto.context}
{
  camera = proto.camera;

  node = new score::gfx::CameraNode(proto.camera, {});
  node_id = context->ui->register_node(std::unique_ptr<score::gfx::CameraNode>{node});

  proto.camera_node = node;
}

void video_texture_input_parameter::pull_texture(ossia::gfx::port_index idx)
{
  context->setEdge(port_index{this->node_id, 0}, idx);

  score::gfx::Message m;
  m.node_id = node_id;
  context->ui->send_message(std::move(m));
}

video_texture_input_parameter::~video_texture_input_parameter()
{
  proto.camera_node = nullptr;
  context->ui->unregister_node(node_id);
}

video_texture_input_node::video_texture_input_node(
    ossia::net::device_base& dev, std::string name)
    : m_device{dev}
    , m_parameter{std::make_unique<video_texture_input_parameter>(
          *this, dynamic_cast<video_texture_input_protocol&>(dev.get_protocol()))}
{
  m_name = std::move(name);
}

video_texture_input_node::~video_texture_input_node() { }

video_texture_input_parameter* video_texture_input_node::get_parameter() const
{
  return m_parameter.get();
}

ossia::net::device_base& video_texture_input_node::get_device() const
{
  return m_device;
}

ossia::net::node_base* video_texture_input_node::get_parent() const
{
  return m_parent;
}

ossia::net::node_base& video_texture_input_node::set_name(std::string)
{
  return *this;
}

ossia::net::parameter_base* video_texture_input_node::create_parameter(ossia::val_type)
{
  return m_parameter.get();
}

bool video_texture_input_node::remove_parameter()
{
  return false;
}

std::unique_ptr<ossia::net::node_base>
video_texture_input_node::make_child(const std::string& name)
{
  return {};
}

void video_texture_input_node::removing_child(ossia::net::node_base& node_base) { }

}
