#pragma once
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/gfx/texture_parameter.hpp>

#include <QLineEdit>

#include <Video/FrameQueue.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxDevice.hpp>
#include <Gfx/Graph/videonode.hpp>
#include <ossia/detail/lockfree_queue.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_node.hpp>

class QComboBox;
namespace libfreenect2
{
class Freenect2Device;
class PacketPipeline;
class SyncMultiFrameListener;
class Frame;
}

namespace Gfx
{
struct kinect2_settings
{
  std::string device;
  bool rgb{};
  bool ir{};
  bool depth{};
};

struct kinect2_camera
{
  kinect2_camera();
  ~kinect2_camera();

  void load(kinect2_settings);
  void start();
  void stop();

  ::Video::FrameQueue rgbFrames;
  ::Video::FrameQueue irFrames;
  ::Video::FrameQueue depthFrames;

private:
  void loop();
  void process(libfreenect2::Frame* colorFrame, libfreenect2::Frame* irFrame, libfreenect2::Frame* depthFrame);

  libfreenect2::Freenect2Device *m_dev{};
  libfreenect2::PacketPipeline *m_pipeline{};
  libfreenect2::SyncMultiFrameListener* m_listener{};
  std::thread m_thread;
  std::atomic_bool m_running{};
  int types{};
};

class kinect2_decoder  : public ::Video::VideoInterface
{
  ::Video::FrameQueue& queue;

public:
  const QString filter;
  kinect2_decoder(::Video::FrameQueue& queue, int width, int height, AVPixelFormat format, QString filter)
    : queue{queue}
    , filter{filter}
  {
    this->width = width;
    this->height = height;
    this->pixel_format = format;
    this->fps = 30;
    this->realTime = true;
    this->dts_per_flicks = 0;
    this->flicks_per_dts = 0;
  }

  ~kinect2_decoder()
  {
    queue.drain();
  }

  AVFrame* dequeue_frame() noexcept override
  {
    return queue.dequeue();
  }

  void release_frame(AVFrame* frame) noexcept override
  {
    return queue.release(frame);
  }
};



class kinect2_protocol : public ossia::net::protocol_base
{
public:
  kinect2_camera kinect2;
  kinect2_protocol(const kinect2_settings& stgs);

  bool pull(ossia::net::parameter_base&) override;
  bool push(const ossia::net::parameter_base&, const ossia::value& v) override;
  bool push_raw(const ossia::net::full_parameter_data&) override;
  bool observe(ossia::net::parameter_base&, bool) override;
  bool update(ossia::net::node_base& node_base) override;

  void start_execution() override;
  void stop_execution() override;
};

class kinect2_parameter : public ossia::gfx::texture_input_parameter
{
  GfxExecutionAction* context{};

public:
  std::shared_ptr<kinect2_decoder> decoder;
  int32_t node_id{};
  VideoNode* node{};

  kinect2_parameter(const std::shared_ptr<kinect2_decoder>& dec, ossia::net::node_base& n, GfxExecutionAction& ctx)
      : ossia::gfx::texture_input_parameter{n}
      , context{&ctx}
      , decoder{dec}
      , node{new VideoNode(decoder, {}, dec->filter)}
  {
    node_id = context->ui->register_node(std::unique_ptr<VideoNode>(node));
  }

  void pull_texture(port_index idx) override
  {
    context->setEdge(port_index{this->node_id, 0}, idx);
  }

  virtual ~kinect2_parameter()
  {
    context->ui->unregister_node(node_id);
  }
};

class kinect2_node : public ossia::net::node_base
{
  ossia::net::device_base& m_device;
  node_base* m_parent{};
  std::unique_ptr<kinect2_parameter> m_parameter;

public:
  kinect2_node(const std::shared_ptr<kinect2_decoder>& settings, GfxExecutionAction& ctx, ossia::net::device_base& dev, std::string name)
      : m_device{dev}
      , m_parameter{std::make_unique<kinect2_parameter>(settings, *this, ctx)}
  {
    m_name = std::move(name);
  }

  kinect2_parameter* get_parameter() const override { return m_parameter.get(); }

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

class kinect2_device : public ossia::net::generic_device
{
public:
  kinect2_device(const kinect2_settings& settings, GfxExecutionAction& ctx, std::unique_ptr<kinect2_protocol> proto, std::string name);
};
}

// Score part

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

namespace Gfx
{
struct Kinect2Settings
{
  QString input;
  bool rgb{true}, ir{true}, depth{true};
};

class Kinect2ProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("1056df8a-f20c-40e4-995e-f18ffda3a16a")
  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  Device::DeviceEnumerator* getEnumerator(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface*
  makeDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;
  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx,
      QWidget* parent) override;
  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&,
      const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx,
      QWidget*) override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor)
      const override;

  bool checkCompatibility(const Device::DeviceSettings& a, const Device::DeviceSettings& b)
      const noexcept override;
};

class Kinect2Device final : public GfxInputDevice
{
  W_OBJECT(Kinect2Device)
public:
  using GfxInputDevice::GfxInputDevice;
  ~Kinect2Device();
private:
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  kinect2_protocol* m_protocol{};
  mutable std::unique_ptr<kinect2_device> m_dev;
};

class Kinect2SettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  Kinect2SettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
  Device::DeviceSettings m_settings;
};

}


Q_DECLARE_METATYPE(Gfx::Kinect2Settings)
W_REGISTER_ARGTYPE(Gfx::Kinect2Settings)
